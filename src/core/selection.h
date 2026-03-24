#pragma once

#include <cstddef>
#include <vector>

namespace berezta {

/// A single selection range defined by an anchor and a head (cursor).
///
/// - anchor: where the selection started.
/// - head: where the cursor currently is.
/// - When anchor == head, the range is a zero-width cursor.
struct Range {
    size_t anchor = 0;
    size_t head = 0;

    Range() = default;
    Range(size_t anchor, size_t head) : anchor(anchor), head(head) {}

    /// Create a zero-width cursor at the given position.
    static Range point(size_t pos) { return {pos, pos}; }

    /// The cursor position (alias for head).
    size_t cursor() const { return head; }

    /// Start of the range (min of anchor, head).
    size_t from() const { return anchor < head ? anchor : head; }

    /// End of the range (max of anchor, head).
    size_t to() const { return anchor > head ? anchor : head; }

    /// Whether this is a zero-width cursor.
    bool is_empty() const { return anchor == head; }

    /// Number of selected characters.
    size_t len() const { return to() - from(); }

    /// Whether two ranges overlap or touch.
    bool overlaps(const Range& other) const {
        return from() <= other.to() && other.from() <= to();
    }

    /// Merge two overlapping ranges. Direction is taken from *this.
    Range merge(const Range& other) const;

    bool operator==(const Range& other) const {
        return anchor == other.anchor && head == other.head;
    }
    bool operator!=(const Range& other) const { return !(*this == other); }
};

/// A set of one or more selection ranges (multi-cursor state).
///
/// Invariants:
/// - Never empty (always at least one range).
/// - Ranges are sorted by start position.
/// - No overlapping ranges (auto-merged).
class Selection {
public:
    /// Default constructor: single cursor at position 0.
    Selection() : ranges_({Range::point(0)}), primary_idx_(0) {}

    /// Create a single zero-width cursor.
    static Selection point(size_t pos);

    /// Create a single range selection.
    static Selection single(size_t anchor, size_t head);

    /// Build from multiple ranges. Normalizes (sorts + merges overlaps).
    static Selection from_ranges(std::vector<Range> ranges, size_t primary_idx);

    /// The primary (main) cursor range.
    const Range& primary() const { return ranges_[primary_idx_]; }

    /// Index of the primary range.
    size_t primary_idx() const { return primary_idx_; }

    /// All ranges.
    const std::vector<Range>& ranges() const { return ranges_; }

    /// Number of cursors.
    size_t len() const { return ranges_.size(); }

    /// Whether there is only one cursor.
    bool is_single() const { return ranges_.size() == 1; }

    /// Add a new range (becomes primary). Re-normalizes.
    Selection push(Range range) const;

    /// Remove range at index. Panics if only one range.
    Selection remove(size_t index) const;

    /// Transform each range, then normalize.
    template <typename Func>
    Selection transform(Func f) const {
        std::vector<Range> new_ranges;
        new_ranges.reserve(ranges_.size());
        for (const auto& r : ranges_) {
            new_ranges.push_back(f(r));
        }
        return from_ranges(std::move(new_ranges), primary_idx_);
    }

    /// Collapse to a single cursor at the primary's head.
    Selection collapse_to_primary() const;

    bool operator==(const Selection& other) const {
        return ranges_ == other.ranges_ && primary_idx_ == other.primary_idx_;
    }

private:
    std::vector<Range> ranges_;
    size_t primary_idx_ = 0;

    /// Sort ranges and merge overlaps.
    void normalize();
};

} // namespace berezta
