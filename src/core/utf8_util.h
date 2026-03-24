#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace berezta::ustr {

/// Count the number of Unicode codepoints in a UTF-8 string.
size_t codepoint_count(const std::string& s);

/// Get the display width of a UTF-8 string in terminal columns.
/// CJK characters = 2, combining marks = 0, normal = 1.
size_t display_width(const std::string& s);

/// Get the display width of a single Unicode codepoint.
int codepoint_width(uint32_t cp);

/// Return the byte offset of the next codepoint after `byte_pos`.
size_t next_codepoint(const std::string& s, size_t byte_pos);

/// Return the byte offset of the codepoint before `byte_pos`.
size_t prev_codepoint(const std::string& s, size_t byte_pos);

/// Return the byte offset of the codepoint at codepoint index `cp_idx`.
size_t codepoint_to_byte(const std::string& s, size_t cp_idx);

/// Return the codepoint index at byte offset `byte_pos`.
size_t byte_to_codepoint(const std::string& s, size_t byte_pos);

/// Extract a substring by codepoint range [cp_from, cp_to).
std::string substr_cp(const std::string& s, size_t cp_from, size_t cp_to);

/// Check if the string is valid UTF-8.
bool is_valid(const std::string& s);

} // namespace berezta::ustr
