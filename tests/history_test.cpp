#include "core/history.h"

#include <gtest/gtest.h>

using namespace berezta;

TEST(HistoryTest, EmptyHistoryCannotUndo) {
    History hist;
    EXPECT_FALSE(hist.can_undo());
    EXPECT_FALSE(hist.can_redo());
    Edit e;
    EXPECT_FALSE(hist.undo(e));
}

TEST(HistoryTest, RecordAndUndo) {
    History hist;
    hist.record(Edit{"", "a", Selection::point(0), Selection::point(1)});
    EXPECT_TRUE(hist.can_undo());

    Edit e;
    EXPECT_TRUE(hist.undo(e));
    EXPECT_EQ(e.buffer_before, "");
    EXPECT_EQ(e.buffer_after, "a");
}

TEST(HistoryTest, UndoThenRedo) {
    History hist;
    hist.record(Edit{"", "hello", Selection::point(0), Selection::point(5)});

    Edit e;
    hist.undo(e);
    EXPECT_TRUE(hist.can_redo());

    Edit e2;
    EXPECT_TRUE(hist.redo(e2));
    EXPECT_EQ(e2.buffer_after, "hello");
}

TEST(HistoryTest, RecordClearsRedoStack) {
    History hist;
    hist.record(Edit{"", "a", Selection::point(0), Selection::point(1)});
    hist.record(Edit{"a", "ab", Selection::point(1), Selection::point(2)});

    Edit e;
    hist.undo(e);
    EXPECT_TRUE(hist.can_redo());

    hist.record(Edit{"a", "ac", Selection::point(1), Selection::point(2)});
    EXPECT_FALSE(hist.can_redo());
}

TEST(HistoryTest, MultipleUndosAndRedos) {
    History hist;
    hist.record(Edit{"", "a", Selection::point(0), Selection::point(1)});
    hist.record(Edit{"a", "ab", Selection::point(1), Selection::point(2)});
    hist.record(Edit{"ab", "abc", Selection::point(2), Selection::point(3)});

    Edit e;
    hist.undo(e);
    EXPECT_EQ(e.buffer_before, "ab");
    hist.undo(e);
    EXPECT_EQ(e.buffer_before, "a");
    hist.undo(e);
    EXPECT_EQ(e.buffer_before, "");
    EXPECT_FALSE(hist.can_undo());

    hist.redo(e);
    EXPECT_EQ(e.buffer_after, "a");
}

TEST(HistoryTest, Clear) {
    History hist;
    hist.record(Edit{"", "a", Selection::point(0), Selection::point(1)});
    hist.clear();
    EXPECT_FALSE(hist.can_undo());
    EXPECT_FALSE(hist.can_redo());
}
