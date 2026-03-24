#include "core/search.h"

#include <string>

namespace berezta {

std::vector<Match> find_all(const std::string& text, const std::string& query) {
    std::vector<Match> matches;
    if (query.empty()) return matches;

    size_t pos = 0;
    while ((pos = text.find(query, pos)) != std::string::npos) {
        matches.push_back({pos, query.size()});
        pos += 1; // allow overlapping matches
    }
    return matches;
}

size_t find_next(const std::string& text, const std::string& query,
                 size_t start, bool wrap) {
    if (query.empty()) return std::string::npos;

    size_t found = text.find(query, start);
    if (found != std::string::npos) return found;

    if (wrap) {
        found = text.find(query, 0);
        if (found < start) return found;
    }
    return std::string::npos;
}

size_t find_prev(const std::string& text, const std::string& query,
                 size_t start, bool wrap) {
    if (query.empty()) return std::string::npos;

    // Search backwards from start.
    size_t found = std::string::npos;
    if (start > 0) {
        found = text.rfind(query, start - 1);
    }
    if (found != std::string::npos) return found;

    if (wrap) {
        found = text.rfind(query);
        if (found != std::string::npos && found >= start) return found;
    }
    return std::string::npos;
}

} // namespace berezta
