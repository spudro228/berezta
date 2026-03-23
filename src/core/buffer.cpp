#include "core/buffer.h"
#include "core/utf8_util.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace beresta {

Buffer::Buffer() : lines_{""}, codec_(std::make_unique<Utf8Codec>()) {}

Buffer::Buffer(const std::string& text) : codec_(std::make_unique<Utf8Codec>()) {
    set_text(text);
}

Buffer Buffer::from_file(const std::string& path, const std::string& encoding) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string raw = ss.str();

    Buffer buf;
    if (encoding == "auto") {
        buf.codec_ = detect_codec(raw);
    } else {
        buf.codec_ = create_codec(encoding);
        if (!buf.codec_) {
            throw std::runtime_error("Unknown encoding: " + encoding);
        }
    }

    std::string utf8_text = buf.codec_->decode(raw);
    buf.set_text(utf8_text);
    return buf;
}

void Buffer::save_to_file(const std::string& path) {
    save_to_file(path, *codec_);
}

void Buffer::save_to_file(const std::string& path, const Codec& save_codec) {
    std::string utf8_text = to_string();
    std::string encoded = save_codec.encode(utf8_text);

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot write to file: " + path);
    }
    file << encoded;
    modified_ = false;
}

size_t Buffer::total_chars() const {
    size_t total = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
        total += lines_[i].size();
        if (i + 1 < lines_.size()) {
            total += 1; // newline separator
        }
    }
    return total;
}

bool Buffer::is_empty() const {
    return lines_.size() == 1 && lines_[0].empty();
}

const std::string& Buffer::line(size_t line_idx) const {
    static const std::string kEmpty;
    if (line_idx >= lines_.size()) {
        return kEmpty;
    }
    return lines_[line_idx];
}

size_t Buffer::line_width(size_t line_idx) const {
    if (line_idx >= lines_.size()) {
        return 0;
    }
    return lines_[line_idx].size();
}

size_t Buffer::line_display_width(size_t line_idx) const {
    if (line_idx >= lines_.size()) {
        return 0;
    }
    return ustr::display_width(lines_[line_idx]);
}

size_t Buffer::pos_to_char(size_t line, size_t col) const {
    line = std::min(line, lines_.size() - 1);
    col = std::min(col, lines_[line].size());

    size_t offset = 0;
    for (size_t i = 0; i < line; ++i) {
        offset += lines_[i].size() + 1; // +1 for newline
    }
    return offset + col;
}

std::pair<size_t, size_t> Buffer::char_to_pos(size_t char_idx) const {
    char_idx = std::min(char_idx, total_chars());
    size_t offset = 0;
    for (size_t i = 0; i < lines_.size(); ++i) {
        size_t line_len = lines_[i].size();
        // If this is the last line, there's no newline after it.
        size_t line_total = (i + 1 < lines_.size()) ? line_len + 1 : line_len;
        if (char_idx <= offset + line_len) {
            return {i, char_idx - offset};
        }
        offset += line_total;
    }
    // Fallback: end of last line.
    return {lines_.size() - 1, lines_.back().size()};
}

std::string Buffer::slice(size_t from, size_t to) const {
    if (from >= to) {
        return "";
    }
    std::string full = to_string();
    from = std::min(from, full.size());
    to = std::min(to, full.size());
    return full.substr(from, to - from);
}

size_t Buffer::display_col_to_byte(size_t line_idx, size_t display_col) const {
    if (line_idx >= lines_.size()) return 0;
    const std::string& ln = lines_[line_idx];
    size_t col = 0;
    auto it = ln.begin();
    auto end = ln.end();
    size_t byte_pos = 0;
    while (it != end && col < display_col) {
        auto prev_it = it;
        uint32_t cp = 0;
        // Manually decode to get the codepoint
        // Use ustr helpers via direct UTF-8 iteration
        auto start = it;
        // We need the codepoint to get its width
        unsigned char c = static_cast<unsigned char>(*it);
        if (c < 0x80) {
            cp = c;
            ++it;
        } else if (c < 0xE0) {
            cp = c & 0x1F;
            if (++it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
        } else if (c < 0xF0) {
            cp = c & 0x0F;
            if (++it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
            if (it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
        } else {
            cp = c & 0x07;
            if (++it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
            if (it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
            if (it != end) { cp = (cp << 6) | (*it & 0x3F); ++it; }
        }
        int w = ustr::codepoint_width(cp);
        if (w <= 0) w = 0;
        if (col + static_cast<size_t>(w) > display_col) {
            // This character straddles the target column; return current byte pos.
            return byte_pos;
        }
        col += static_cast<size_t>(w);
        byte_pos = static_cast<size_t>(it - ln.begin());
    }
    return byte_pos;
}

size_t Buffer::byte_to_display_col(size_t line_idx, size_t byte_offset_in_line) const {
    if (line_idx >= lines_.size()) return 0;
    const std::string& ln = lines_[line_idx];
    byte_offset_in_line = std::min(byte_offset_in_line, ln.size());
    if (byte_offset_in_line == 0) return 0;
    // Compute display width of the substring [0, byte_offset_in_line).
    std::string sub = ln.substr(0, byte_offset_in_line);
    return ustr::display_width(sub);
}

void Buffer::insert(size_t char_idx, const std::string& text) {
    if (text.empty()) {
        return;
    }
    std::string full = to_string();
    char_idx = std::min(char_idx, full.size());
    full.insert(char_idx, text);
    set_text(full);
    modified_ = true;
}

void Buffer::remove(size_t from, size_t to) {
    if (from >= to) {
        return;
    }
    std::string full = to_string();
    from = std::min(from, full.size());
    to = std::min(to, full.size());
    if (from >= to) {
        return;
    }
    full.erase(from, to - from);
    set_text(full);
    modified_ = true;
}

void Buffer::replace(size_t from, size_t to, const std::string& text) {
    std::string full = to_string();
    from = std::min(from, full.size());
    to = std::min(to, full.size());
    if (from > to) {
        from = to;
    }
    full.erase(from, to - from);
    full.insert(from, text);
    set_text(full);
    modified_ = true;
}

std::string Buffer::to_string() const {
    std::string result;
    for (size_t i = 0; i < lines_.size(); ++i) {
        result += lines_[i];
        if (i + 1 < lines_.size()) {
            result += '\n';
        }
    }
    return result;
}

const Codec& Buffer::codec() const {
    return *codec_;
}

void Buffer::set_codec(std::unique_ptr<Codec> new_codec) {
    if (new_codec) {
        codec_ = std::move(new_codec);
    }
}

void Buffer::set_text(const std::string& text) {
    lines_.clear();
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        // Strip trailing \r (Windows line endings).
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines_.push_back(std::move(line));
    }
    // If text ends with newline, add an empty trailing line.
    if (!text.empty() && text.back() == '\n') {
        lines_.emplace_back("");
    }
    // Buffer always has at least one line.
    if (lines_.empty()) {
        lines_.emplace_back("");
    }
}

} // namespace beresta
