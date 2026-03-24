#include "core/utf8_util.h"

#include <algorithm>
#include <utf8.h>
#include <wcwidth.h>

namespace berezta::ustr {

size_t codepoint_count(const std::string& s) {
    return utf8::distance(s.begin(), s.end());
}

int codepoint_width(uint32_t cp) {
    int w = mk_wcwidth(static_cast<int>(cp));
    return w < 0 ? 0 : w; // treat control chars as 0-width
}

size_t display_width(const std::string& s) {
    size_t width = 0;
    auto it = s.begin();
    auto end = s.end();
    while (it != end) {
        uint32_t cp = utf8::next(it, end);
        width += codepoint_width(cp);
    }
    return width;
}

size_t next_codepoint(const std::string& s, size_t byte_pos) {
    if (byte_pos >= s.size()) return s.size();
    auto it = s.begin() + byte_pos;
    utf8::next(it, s.end());
    return static_cast<size_t>(it - s.begin());
}

size_t prev_codepoint(const std::string& s, size_t byte_pos) {
    if (byte_pos == 0) return 0;
    auto it = s.begin() + byte_pos;
    utf8::prior(it, s.begin());
    return static_cast<size_t>(it - s.begin());
}

size_t codepoint_to_byte(const std::string& s, size_t cp_idx) {
    auto it = s.begin();
    auto end = s.end();
    for (size_t i = 0; i < cp_idx && it != end; ++i) {
        utf8::next(it, end);
    }
    return static_cast<size_t>(it - s.begin());
}

size_t byte_to_codepoint(const std::string& s, size_t byte_pos) {
    byte_pos = std::min(byte_pos, s.size());
    return utf8::distance(s.begin(), s.begin() + byte_pos);
}

std::string substr_cp(const std::string& s, size_t cp_from, size_t cp_to) {
    size_t byte_from = codepoint_to_byte(s, cp_from);
    size_t byte_to = codepoint_to_byte(s, cp_to);
    if (byte_from >= byte_to) return "";
    return s.substr(byte_from, byte_to - byte_from);
}

bool is_valid(const std::string& s) {
    return utf8::is_valid(s.begin(), s.end());
}

} // namespace berezta::ustr
