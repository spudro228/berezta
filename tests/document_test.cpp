#include "core/document.h"

#include <gtest/gtest.h>

using namespace beresta;

namespace {
Document make_doc(const std::string& text) {
    Document doc(80, 24);
    doc.buffer = Buffer(text);
    return doc;
}
} // namespace

TEST(DocumentTest, NewDocumentStartsAtOrigin) {
    Document doc(80, 24);
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
    EXPECT_EQ(doc.viewport.scroll_line, 0);
    EXPECT_EQ(doc.viewport.scroll_col, 0);
}

TEST(DocumentTest, TitleWithFilePath) {
    Document doc(80, 24);
    doc.file_path = "/home/user/test.json";
    EXPECT_EQ(doc.title(), "test.json");
}

TEST(DocumentTest, TitleWithoutFilePath) {
    Document doc(80, 24);
    EXPECT_EQ(doc.title(), "[untitled]");
}

TEST(DocumentTest, MoveLeftAtStartStays) {
    auto doc = make_doc("hello");
    doc.move_left();
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
}

TEST(DocumentTest, MoveRightAdvancesCursor) {
    auto doc = make_doc("hello");
    doc.move_right();
    EXPECT_EQ(doc.selection.primary().cursor(), 1);
}

TEST(DocumentTest, MoveRightAtEndStays) {
    auto doc = make_doc("hi");
    doc.move_right();
    doc.move_right();
    doc.move_right(); // past end
    EXPECT_EQ(doc.selection.primary().cursor(), 2);
}

TEST(DocumentTest, MoveDownAndUp) {
    auto doc = make_doc("abc\ndef\nghi");
    doc.move_right(); // col 1
    doc.move_down();
    auto [line, col] = doc.buffer.char_to_pos(doc.selection.primary().cursor());
    EXPECT_EQ(line, 1);
    EXPECT_EQ(col, 1);

    doc.move_up();
    auto [line2, col2] = doc.buffer.char_to_pos(doc.selection.primary().cursor());
    EXPECT_EQ(line2, 0);
    EXPECT_EQ(col2, 1);
}

TEST(DocumentTest, MoveUpAtFirstLineGoesToStart) {
    auto doc = make_doc("abc\ndef");
    doc.move_right();
    doc.move_up();
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
}

TEST(DocumentTest, MoveDownAtLastLineGoesToEnd) {
    auto doc = make_doc("abc\ndef");
    doc.move_down();
    doc.move_down();
    EXPECT_EQ(doc.selection.primary().cursor(), doc.buffer.total_chars());
}

TEST(DocumentTest, MoveLineStartAndEnd) {
    auto doc = make_doc("hello world");
    doc.move_right();
    doc.move_right();
    doc.move_line_start();
    EXPECT_EQ(doc.selection.primary().cursor(), 0);

    doc.move_line_end();
    EXPECT_EQ(doc.selection.primary().cursor(), 11);
}

TEST(DocumentTest, MoveDocStartAndEnd) {
    auto doc = make_doc("abc\ndef\nghi");
    doc.move_doc_end();
    EXPECT_EQ(doc.selection.primary().cursor(), doc.buffer.total_chars());

    doc.move_doc_start();
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
}

TEST(DocumentTest, EnsureCursorVisibleScrollsDown) {
    // Create a document with many lines.
    std::string text;
    for (int i = 0; i < 100; ++i) {
        text += "line\n";
    }
    auto doc = make_doc(text);
    doc.resize_viewport(80, 10);

    // Place cursor at line 50.
    size_t pos = doc.buffer.pos_to_char(50, 0);
    doc.selection = Selection::point(pos);
    doc.ensure_cursor_visible();

    EXPECT_LE(doc.viewport.scroll_line, 50u);
    EXPECT_GT(doc.viewport.scroll_line + doc.viewport.height, 50u);
}

TEST(DocumentTest, PageUpAndDown) {
    std::string text;
    for (int i = 0; i < 100; ++i) {
        text += "line\n";
    }
    auto doc = make_doc(text);
    doc.resize_viewport(80, 10);

    doc.page_down();
    auto [line, _] = doc.buffer.char_to_pos(doc.selection.primary().cursor());
    EXPECT_EQ(line, 9); // page = height - 1 = 9

    doc.page_up();
    auto [line2, __] = doc.buffer.char_to_pos(doc.selection.primary().cursor());
    EXPECT_EQ(line2, 0);
}

TEST(DocumentTest, ResizeViewport) {
    auto doc = make_doc("hello");
    doc.resize_viewport(40, 12);
    EXPECT_EQ(doc.viewport.width, 40);
    EXPECT_EQ(doc.viewport.height, 12);
}

// --- Text editing tests ---

TEST(DocumentTest, InsertChar) {
    auto doc = make_doc("");
    doc.insert_char('h');
    doc.insert_char('i');
    EXPECT_EQ(doc.buffer.to_string(), "hi");
    EXPECT_EQ(doc.selection.primary().cursor(), 2);
}

TEST(DocumentTest, InsertNewline) {
    auto doc = make_doc("ab");
    doc.move_right(); // cursor at 1
    doc.insert_newline();
    EXPECT_EQ(doc.buffer.to_string(), "a\nb");
    EXPECT_EQ(doc.buffer.line_count(), 2);
}

TEST(DocumentTest, DeleteBackward) {
    auto doc = make_doc("abc");
    doc.move_right();
    doc.move_right(); // cursor at 2
    doc.delete_backward();
    EXPECT_EQ(doc.buffer.to_string(), "ac");
    EXPECT_EQ(doc.selection.primary().cursor(), 1);
}

TEST(DocumentTest, DeleteBackwardAtStart) {
    auto doc = make_doc("abc");
    doc.delete_backward(); // cursor at 0 — nothing happens
    EXPECT_EQ(doc.buffer.to_string(), "abc");
}

TEST(DocumentTest, DeleteForward) {
    auto doc = make_doc("abc");
    doc.move_right(); // cursor at 1
    doc.delete_forward();
    EXPECT_EQ(doc.buffer.to_string(), "ac");
    EXPECT_EQ(doc.selection.primary().cursor(), 1);
}

TEST(DocumentTest, DeleteForwardAtEnd) {
    auto doc = make_doc("abc");
    doc.move_doc_end();
    doc.delete_forward(); // nothing happens
    EXPECT_EQ(doc.buffer.to_string(), "abc");
}

TEST(DocumentTest, DeleteSelectedText) {
    auto doc = make_doc("hello world");
    doc.select_right(); // select 'h'
    doc.select_right(); // select 'e'
    doc.select_right(); // select 'l'
    doc.select_right(); // select 'l'
    doc.select_right(); // select 'o'
    doc.delete_backward(); // delete selection
    EXPECT_EQ(doc.buffer.to_string(), " world");
}

TEST(DocumentTest, InsertReplacesSelection) {
    auto doc = make_doc("hello");
    doc.select_right();
    doc.select_right();
    doc.select_right(); // select "hel"
    doc.insert_char('X');
    EXPECT_EQ(doc.buffer.to_string(), "Xlo");
}

TEST(DocumentTest, UndoInsert) {
    auto doc = make_doc("ab");
    doc.move_doc_end();
    doc.insert_char('c');
    EXPECT_EQ(doc.buffer.to_string(), "abc");

    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "ab");
    EXPECT_EQ(doc.selection.primary().cursor(), 2); // restored
}

TEST(DocumentTest, UndoDelete) {
    auto doc = make_doc("abc");
    doc.move_right(); // cursor at 1
    doc.delete_forward(); // delete 'b'
    EXPECT_EQ(doc.buffer.to_string(), "ac");

    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "abc");
}

TEST(DocumentTest, RedoAfterUndo) {
    auto doc = make_doc("ab");
    doc.move_doc_end();
    doc.insert_char('c');
    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "ab");

    doc.redo();
    EXPECT_EQ(doc.buffer.to_string(), "abc");
}

TEST(DocumentTest, MultipleUndos) {
    auto doc = make_doc("");
    doc.insert_char('a');
    doc.insert_char('b');
    doc.insert_char('c');
    EXPECT_EQ(doc.buffer.to_string(), "abc");

    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "ab");
    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "a");
    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "");
}

TEST(DocumentTest, UndoNewlineInsertion) {
    auto doc = make_doc("ab");
    doc.move_right();
    doc.insert_newline();
    EXPECT_EQ(doc.buffer.to_string(), "a\nb");

    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "ab");
}

// --- Word movement tests ---

TEST(DocumentTest, MoveWordRight) {
    auto doc = make_doc("hello world foo");
    doc.move_word_right();
    // Should skip "hello" and whitespace, land at 'w'
    EXPECT_EQ(doc.selection.primary().cursor(), 6);

    doc.move_word_right();
    EXPECT_EQ(doc.selection.primary().cursor(), 12);
}

TEST(DocumentTest, MoveWordLeft) {
    auto doc = make_doc("hello world");
    doc.move_doc_end(); // cursor at 11
    doc.move_word_left();
    EXPECT_EQ(doc.selection.primary().cursor(), 6); // start of "world"

    doc.move_word_left();
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
}

// --- Selection tests ---

TEST(DocumentTest, SelectLeftAndRight) {
    auto doc = make_doc("abc");
    doc.select_right();
    doc.select_right();
    EXPECT_EQ(doc.selection.primary().anchor, 0);
    EXPECT_EQ(doc.selection.primary().head, 2);
    EXPECT_EQ(doc.selection.primary().len(), 2);
}

TEST(DocumentTest, SelectAll) {
    auto doc = make_doc("hello");
    doc.select_all();
    EXPECT_EQ(doc.selection.primary().from(), 0);
    EXPECT_EQ(doc.selection.primary().to(), 5);
}

TEST(DocumentTest, MoveCollapseSelection) {
    auto doc = make_doc("abcdef");
    doc.select_right();
    doc.select_right();
    doc.select_right(); // selected "abc"

    doc.move_left(); // should collapse to start of selection
    EXPECT_EQ(doc.selection.primary().cursor(), 0);
    EXPECT_TRUE(doc.selection.primary().is_empty());
}

TEST(DocumentTest, MoveRightCollapseSelection) {
    auto doc = make_doc("abcdef");
    doc.select_right();
    doc.select_right(); // selected "ab"

    doc.move_right(); // should collapse to end of selection
    EXPECT_EQ(doc.selection.primary().cursor(), 2);
    EXPECT_TRUE(doc.selection.primary().is_empty());
}

// --- Multi-cursor tests ---

TEST(DocumentTest, AddCursorBelow) {
    auto doc = make_doc("aaa\nbbb\nccc");
    doc.move_right(); // col 1
    doc.add_cursor_below();
    EXPECT_EQ(doc.selection.len(), 2);
    // First cursor at (0,1)=1, second at (1,1)=5
    auto& ranges = doc.selection.ranges();
    EXPECT_EQ(ranges[0].cursor(), 1);
    EXPECT_EQ(ranges[1].cursor(), 5);
}

TEST(DocumentTest, AddCursorAbove) {
    auto doc = make_doc("aaa\nbbb\nccc");
    // Move to line 2, col 1
    doc.move_down();
    doc.move_down();
    doc.move_right();
    doc.add_cursor_above();
    EXPECT_EQ(doc.selection.len(), 2);
}

TEST(DocumentTest, MultiCursorInsert) {
    auto doc = make_doc("aaa\nbbb\nccc");
    // Place cursors at start of each line
    doc.add_cursor_below();
    doc.add_cursor_below();
    EXPECT_EQ(doc.selection.len(), 3);

    doc.insert_char('X');
    EXPECT_EQ(doc.buffer.to_string(), "Xaaa\nXbbb\nXccc");
}

TEST(DocumentTest, MultiCursorDelete) {
    auto doc = make_doc("Xaaa\nXbbb\nXccc");
    // Place cursors at position 1 of each line
    doc.move_right(); // after X on line 0
    doc.add_cursor_below();
    doc.add_cursor_below();
    EXPECT_EQ(doc.selection.len(), 3);

    doc.delete_backward();
    EXPECT_EQ(doc.buffer.to_string(), "aaa\nbbb\nccc");
}

TEST(DocumentTest, MultiCursorUndoIsAtomic) {
    auto doc = make_doc("aaa\nbbb\nccc");
    doc.add_cursor_below();
    doc.add_cursor_below();
    doc.insert_char('X');
    EXPECT_EQ(doc.buffer.to_string(), "Xaaa\nXbbb\nXccc");

    doc.undo();
    EXPECT_EQ(doc.buffer.to_string(), "aaa\nbbb\nccc");
    // Selection is restored to 3 cursors
    EXPECT_EQ(doc.selection.len(), 3);
}

TEST(DocumentTest, SelectNextOccurrence) {
    auto doc = make_doc("foo bar foo baz foo");
    // Select "foo" at the beginning
    doc.select_right();
    doc.select_right();
    doc.select_right(); // selected "foo"

    doc.select_next_occurrence();
    EXPECT_EQ(doc.selection.len(), 2);
    // Second selection should be at position 8-11 ("foo" after "bar ")
    auto& ranges = doc.selection.ranges();
    EXPECT_EQ(ranges[1].from(), 8);
    EXPECT_EQ(ranges[1].to(), 11);

    doc.select_next_occurrence();
    EXPECT_EQ(doc.selection.len(), 3);
    EXPECT_EQ(ranges[2].from(), 16);
    EXPECT_EQ(ranges[2].to(), 19);
}

TEST(DocumentTest, SelectNextOccurrenceWrapsAround) {
    auto doc = make_doc("foo bar foo");
    // Move to the second "foo" and select it
    for (int i = 0; i < 8; ++i) doc.move_right();
    doc.select_right();
    doc.select_right();
    doc.select_right(); // selected second "foo"

    doc.select_next_occurrence(); // should wrap to first "foo"
    EXPECT_EQ(doc.selection.len(), 2);
    EXPECT_EQ(doc.selection.ranges()[0].from(), 0);
}

TEST(DocumentTest, SelectNextOccurrenceByWord) {
    auto doc = make_doc("cat dog cat bird cat");
    // Cursor on "cat" — no selection, should detect word and find next
    doc.select_next_occurrence();
    EXPECT_EQ(doc.selection.len(), 2);
    // First "cat" at 0-3, second at 8-11
    EXPECT_EQ(doc.selection.ranges()[0].from(), 0);
    EXPECT_EQ(doc.selection.ranges()[1].from(), 8);
}

TEST(DocumentTest, DeselectLastCursor) {
    auto doc = make_doc("aaa\nbbb\nccc");
    doc.add_cursor_below();
    doc.add_cursor_below();
    EXPECT_EQ(doc.selection.len(), 3);

    doc.deselect_last_cursor();
    EXPECT_EQ(doc.selection.len(), 2);

    doc.deselect_last_cursor();
    EXPECT_EQ(doc.selection.len(), 1);

    doc.deselect_last_cursor(); // can't remove last
    EXPECT_EQ(doc.selection.len(), 1);
}

TEST(DocumentTest, MultiCursorTypeAndUndo) {
    auto doc = make_doc("aa\nbb");
    doc.add_cursor_below();
    doc.insert_char('1');
    doc.insert_char('2');
    EXPECT_EQ(doc.buffer.to_string(), "12aa\n12bb");

    doc.undo(); // undo '2'
    EXPECT_EQ(doc.buffer.to_string(), "1aa\n1bb");
    doc.undo(); // undo '1'
    EXPECT_EQ(doc.buffer.to_string(), "aa\nbb");
}
