#include "core/selection.h"

#include <algorithm>
#include <cassert>
#include <numeric>

namespace berezta {

Range Range::merge(const Range& other) const {
    size_t new_from = std::min(from(), other.from());
    size_t new_to = std::max(to(), other.to());
    // Preserve direction of *this.
    if (anchor > head) {
        return {new_to, new_from};
    }
    return {new_from, new_to};
}

// --- Selection ---

Selection Selection::point(size_t pos) {
    Selection sel;
    sel.ranges_.clear();
    sel.ranges_.push_back(Range::point(pos));
    sel.primary_idx_ = 0;
    return sel;
}

Selection Selection::single(size_t anchor, size_t head) {
    Selection sel;
    sel.ranges_.clear();
    sel.ranges_.push_back(Range(anchor, head));
    sel.primary_idx_ = 0;
    return sel;
}

Selection Selection::from_ranges(std::vector<Range> ranges, size_t primary_idx) {
    Selection sel;
    sel.ranges_ = std::move(ranges);
    sel.primary_idx_ = std::min(primary_idx, sel.ranges_.empty() ? 0 : sel.ranges_.size() - 1);
    sel.normalize();
    return sel;
}

Selection Selection::push(Range range) const {
    std::vector<Range> new_ranges = ranges_;
    new_ranges.push_back(range);
    // New range becomes primary.
    return from_ranges(std::move(new_ranges), new_ranges.size() - 1);
}

Selection Selection::remove(size_t index) const {
    assert(ranges_.size() > 1 && "Cannot remove the last range from a selection");
    assert(index < ranges_.size());

    std::vector<Range> new_ranges;
    new_ranges.reserve(ranges_.size() - 1);
    for (size_t i = 0; i < ranges_.size(); ++i) {
        if (i != index) {
            new_ranges.push_back(ranges_[i]);
        }
    }

    size_t new_primary = primary_idx_;
    if (index < primary_idx_) {
        new_primary--;
    } else if (new_primary >= new_ranges.size()) {
        new_primary = new_ranges.size() - 1;
    }

    Selection sel;
    sel.ranges_ = std::move(new_ranges);
    sel.primary_idx_ = new_primary;
    return sel;
}

Selection Selection::collapse_to_primary() const {
    return Selection::point(primary().cursor());
}

void Selection::normalize() {
    if (ranges_.empty()) {
        ranges_.push_back(Range::point(0));
        primary_idx_ = 0;
        return;
    }

    // Build indices sorted by range start position.
    std::vector<size_t> indices(ranges_.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
        return ranges_[a].from() < ranges_[b].from();
    });

    // Track the primary range through merging.
    Range primary_range = ranges_[primary_idx_];

    std::vector<Range> merged;
    merged.reserve(ranges_.size());
    size_t new_primary = 0;

    for (size_t idx : indices) {
        Range range = ranges_[idx];
        if (!merged.empty() && merged.back().overlaps(range)) {
            merged.back() = merged.back().merge(range);
            if (merged.back().overlaps(primary_range)) {
                new_primary = merged.size() - 1;
            }
        } else {
            if (idx == primary_idx_ || range == primary_range) {
                new_primary = merged.size();
            }
            merged.push_back(range);
        }
    }

    ranges_ = std::move(merged);
    primary_idx_ = std::min(new_primary, ranges_.size() - 1);
}

} // namespace berezta
