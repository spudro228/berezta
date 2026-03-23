#include "core/selection.h"

#include <gtest/gtest.h>

using namespace beresta;

// --- Range tests ---

TEST(RangeTest, PointIsEmpty) {
    auto r = Range::point(5);
    EXPECT_TRUE(r.is_empty());
    EXPECT_EQ(r.len(), 0);
    EXPECT_EQ(r.from(), 5);
    EXPECT_EQ(r.to(), 5);
}

TEST(RangeTest, ForwardSelection) {
    Range r(2, 8);
    EXPECT_FALSE(r.is_empty());
    EXPECT_EQ(r.from(), 2);
    EXPECT_EQ(r.to(), 8);
    EXPECT_EQ(r.len(), 6);
    EXPECT_EQ(r.cursor(), 8);
}

TEST(RangeTest, ReversedSelection) {
    Range r(8, 2);
    EXPECT_EQ(r.from(), 2);
    EXPECT_EQ(r.to(), 8);
    EXPECT_EQ(r.cursor(), 2);
}

TEST(RangeTest, OverlapDetection) {
    Range a(0, 5);
    Range b(3, 8);
    EXPECT_TRUE(a.overlaps(b));
    EXPECT_TRUE(b.overlaps(a));

    Range c(6, 10);
    EXPECT_FALSE(a.overlaps(c));
}

TEST(RangeTest, AdjacentOverlaps) {
    Range a(0, 5);
    Range b(5, 10);
    EXPECT_TRUE(a.overlaps(b));
}

TEST(RangeTest, Merge) {
    Range a(0, 5);
    Range b(3, 8);
    auto merged = a.merge(b);
    EXPECT_EQ(merged.from(), 0);
    EXPECT_EQ(merged.to(), 8);
}

// --- Selection tests ---

TEST(SelectionTest, Point) {
    auto sel = Selection::point(10);
    EXPECT_EQ(sel.len(), 1);
    EXPECT_TRUE(sel.is_single());
    EXPECT_EQ(sel.primary().cursor(), 10);
}

TEST(SelectionTest, SingleRange) {
    auto sel = Selection::single(5, 10);
    EXPECT_EQ(sel.primary().from(), 5);
    EXPECT_EQ(sel.primary().to(), 10);
}

TEST(SelectionTest, PushAddsCursor) {
    auto sel = Selection::point(0);
    sel = sel.push(Range::point(10));
    EXPECT_EQ(sel.len(), 2);
    EXPECT_EQ(sel.ranges()[0].cursor(), 0);
    EXPECT_EQ(sel.ranges()[1].cursor(), 10);
}

TEST(SelectionTest, PushMaintainsSort) {
    auto sel = Selection::point(10);
    sel = sel.push(Range::point(5));
    EXPECT_EQ(sel.ranges()[0].cursor(), 5);
    EXPECT_EQ(sel.ranges()[1].cursor(), 10);
}

TEST(SelectionTest, PushMergesOverlapping) {
    auto sel = Selection::single(0, 5);
    sel = sel.push(Range(3, 8));
    EXPECT_EQ(sel.len(), 1);
    EXPECT_EQ(sel.primary().from(), 0);
    EXPECT_EQ(sel.primary().to(), 8);
}

TEST(SelectionTest, Remove) {
    auto sel = Selection::point(0);
    sel = sel.push(Range::point(10));
    sel = sel.push(Range::point(20));
    EXPECT_EQ(sel.len(), 3);
    sel = sel.remove(1);
    EXPECT_EQ(sel.len(), 2);
    EXPECT_EQ(sel.ranges()[0].cursor(), 0);
    EXPECT_EQ(sel.ranges()[1].cursor(), 20);
}

TEST(SelectionTest, Transform) {
    auto sel = Selection::point(5);
    sel = sel.push(Range::point(10));
    sel = sel.transform([](Range r) {
        return Range::point(r.cursor() + 1);
    });
    EXPECT_EQ(sel.ranges()[0].cursor(), 6);
    EXPECT_EQ(sel.ranges()[1].cursor(), 11);
}

TEST(SelectionTest, CollapseToPrimary) {
    auto sel = Selection::point(5);
    sel = sel.push(Range::point(10));
    sel = sel.push(Range::point(15));
    auto collapsed = sel.collapse_to_primary();
    EXPECT_EQ(collapsed.len(), 1);
}

TEST(SelectionTest, FromRangesNormalizes) {
    std::vector<Range> ranges = {
        Range(10, 15),
        Range(0, 5),
        Range(3, 8),
    };
    auto sel = Selection::from_ranges(std::move(ranges), 0);
    // [0,5) and [3,8) overlap → merged to [0,8), then [10,15) is separate.
    EXPECT_EQ(sel.len(), 2);
    EXPECT_EQ(sel.ranges()[0].from(), 0);
    EXPECT_EQ(sel.ranges()[0].to(), 8);
    EXPECT_EQ(sel.ranges()[1].from(), 10);
    EXPECT_EQ(sel.ranges()[1].to(), 15);
}
