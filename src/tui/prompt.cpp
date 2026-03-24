#include "tui/prompt.h"

#include <algorithm>

namespace berezta {

namespace {

/// Find the start of the previous UTF-8 character before `pos`.
size_t prev_utf8_pos(const std::string& s, size_t pos) {
    if (pos == 0) return 0;
    pos--;
    // Walk back over continuation bytes (10xxxxxx).
    while (pos > 0 && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
        pos--;
    }
    return pos;
}

/// Find the start of the next UTF-8 character after `pos`.
size_t next_utf8_pos(const std::string& s, size_t pos) {
    if (pos >= s.size()) return s.size();
    pos++;
    while (pos < s.size() && (static_cast<unsigned char>(s[pos]) & 0xC0) == 0x80) {
        pos++;
    }
    return pos;
}

} // anonymous namespace

void Prompt::insert_char(char ch) {
    input_.insert(cursor_, 1, ch);
    cursor_++;
}

void Prompt::delete_backward() {
    if (cursor_ > 0) {
        size_t prev = prev_utf8_pos(input_, cursor_);
        input_.erase(prev, cursor_ - prev);
        cursor_ = prev;
    }
}

void Prompt::delete_forward() {
    if (cursor_ < input_.size()) {
        size_t next = next_utf8_pos(input_, cursor_);
        input_.erase(cursor_, next - cursor_);
    }
}

void Prompt::move_left() {
    cursor_ = prev_utf8_pos(input_, cursor_);
}

void Prompt::move_right() {
    cursor_ = next_utf8_pos(input_, cursor_);
}

void Prompt::move_home() {
    cursor_ = 0;
}

void Prompt::move_end() {
    cursor_ = input_.size();
}

void Prompt::clear() {
    input_.clear();
    cursor_ = 0;
}

void Prompt::set_input(const std::string& text) {
    input_ = text;
    cursor_ = text.size();
}

} // namespace berezta
