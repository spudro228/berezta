#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace berezta {

/// A single search match: start position and length.
struct Match {
    size_t pos;
    size_t len;
};

/// Find all occurrences of `query` in `text`.
/// Returns matches sorted by position.
std::vector<Match> find_all(const std::string& text, const std::string& query);

/// Find the next occurrence at or after `start`.
/// Returns the position or std::string::npos if not found.
/// If `wrap` is true, wraps around to the beginning.
size_t find_next(const std::string& text, const std::string& query,
                 size_t start, bool wrap = true);

/// Find the previous occurrence before `start`.
/// If `wrap` is true, wraps around to the end.
size_t find_prev(const std::string& text, const std::string& query,
                 size_t start, bool wrap = true);

} // namespace berezta
