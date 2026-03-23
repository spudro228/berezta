#include "core/buffer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace beresta;

TEST(BufferTest, DefaultConstructorCreatesEmptyBuffer) {
    Buffer buf;
    EXPECT_TRUE(buf.is_empty());
    EXPECT_EQ(buf.line_count(), 1);
    EXPECT_EQ(buf.total_chars(), 0);
    EXPECT_FALSE(buf.is_modified());
}

TEST(BufferTest, ConstructFromString) {
    Buffer buf("hello\nworld");
    EXPECT_FALSE(buf.is_empty());
    EXPECT_EQ(buf.line_count(), 2);
    EXPECT_EQ(buf.total_chars(), 11);
    EXPECT_EQ(buf.line(0), "hello");
    EXPECT_EQ(buf.line(1), "world");
}

TEST(BufferTest, ConstructFromStringWithTrailingNewline) {
    Buffer buf("abc\ndef\n");
    EXPECT_EQ(buf.line_count(), 3);
    EXPECT_EQ(buf.line(0), "abc");
    EXPECT_EQ(buf.line(1), "def");
    EXPECT_EQ(buf.line(2), "");
}

TEST(BufferTest, LineWidth) {
    Buffer buf("hello\nhi\n");
    EXPECT_EQ(buf.line_width(0), 5);
    EXPECT_EQ(buf.line_width(1), 2);
    EXPECT_EQ(buf.line_width(2), 0);
}

TEST(BufferTest, PosToCharBasic) {
    Buffer buf("abc\ndef\nghi");
    EXPECT_EQ(buf.pos_to_char(0, 0), 0);
    EXPECT_EQ(buf.pos_to_char(0, 2), 2);
    EXPECT_EQ(buf.pos_to_char(1, 0), 4); // "abc\n" = 4 chars
    EXPECT_EQ(buf.pos_to_char(1, 2), 6);
    EXPECT_EQ(buf.pos_to_char(2, 1), 9);
}

TEST(BufferTest, PosToCharClampsColumn) {
    Buffer buf("ab\ncd");
    EXPECT_EQ(buf.pos_to_char(0, 99), 2); // clamp to "ab" length
}

TEST(BufferTest, PosToCharClampsLine) {
    Buffer buf("ab\ncd");
    EXPECT_EQ(buf.pos_to_char(99, 0), 3); // clamp to last line start
}

TEST(BufferTest, CharToPosBasic) {
    Buffer buf("abc\ndef");
    EXPECT_EQ(buf.char_to_pos(0), std::make_pair(size_t(0), size_t(0)));
    EXPECT_EQ(buf.char_to_pos(2), std::make_pair(size_t(0), size_t(2)));
    EXPECT_EQ(buf.char_to_pos(4), std::make_pair(size_t(1), size_t(0)));
    EXPECT_EQ(buf.char_to_pos(6), std::make_pair(size_t(1), size_t(2)));
}

TEST(BufferTest, CharToPosClamps) {
    Buffer buf("abc");
    EXPECT_EQ(buf.char_to_pos(100), std::make_pair(size_t(0), size_t(3)));
}

TEST(BufferTest, SliceBasic) {
    Buffer buf("hello world");
    EXPECT_EQ(buf.slice(0, 5), "hello");
    EXPECT_EQ(buf.slice(6, 11), "world");
}

TEST(BufferTest, SliceEmptyRange) {
    Buffer buf("hello");
    EXPECT_EQ(buf.slice(3, 3), "");
    EXPECT_EQ(buf.slice(5, 3), ""); // inverted range
}

TEST(BufferTest, SliceClampsToEnd) {
    Buffer buf("hello");
    EXPECT_EQ(buf.slice(3, 100), "lo");
}

TEST(BufferTest, InsertAtBeginning) {
    Buffer buf("world");
    buf.insert(0, "hello ");
    EXPECT_EQ(buf.to_string(), "hello world");
    EXPECT_TRUE(buf.is_modified());
}

TEST(BufferTest, InsertAtEnd) {
    Buffer buf("hello");
    buf.insert(5, " world");
    EXPECT_EQ(buf.to_string(), "hello world");
}

TEST(BufferTest, InsertInMiddle) {
    Buffer buf("helo");
    buf.insert(2, "l");
    EXPECT_EQ(buf.to_string(), "hello");
}

TEST(BufferTest, InsertClamps) {
    Buffer buf("abc");
    buf.insert(100, "d");
    EXPECT_EQ(buf.to_string(), "abcd");
}

TEST(BufferTest, InsertNewline) {
    Buffer buf("ab");
    buf.insert(1, "\n");
    EXPECT_EQ(buf.line_count(), 2);
    EXPECT_EQ(buf.line(0), "a");
    EXPECT_EQ(buf.line(1), "b");
}

TEST(BufferTest, RemoveRange) {
    Buffer buf("hello world");
    buf.remove(5, 11);
    EXPECT_EQ(buf.to_string(), "hello");
    EXPECT_TRUE(buf.is_modified());
}

TEST(BufferTest, RemoveEmptyRange) {
    Buffer buf("hello");
    buf.remove(3, 3);
    EXPECT_EQ(buf.to_string(), "hello");
    EXPECT_FALSE(buf.is_modified());
}

TEST(BufferTest, RemoveClampsToEnd) {
    Buffer buf("hello");
    buf.remove(3, 100);
    EXPECT_EQ(buf.to_string(), "hel");
}

TEST(BufferTest, ReplaceText) {
    Buffer buf("hello world");
    buf.replace(0, 5, "goodbye");
    EXPECT_EQ(buf.to_string(), "goodbye world");
}

TEST(BufferTest, ToString) {
    Buffer buf("abc\ndef\nghi");
    EXPECT_EQ(buf.to_string(), "abc\ndef\nghi");
}

TEST(BufferTest, ToStringWithTrailingNewline) {
    Buffer buf("abc\n");
    EXPECT_EQ(buf.to_string(), "abc\n");
}

TEST(BufferTest, SaveAndLoadRoundtrip) {
    auto path = std::filesystem::temp_directory_path() / "beresta_test_buffer.txt";
    std::string content = "test content\nline two";

    {
        Buffer buf(content);
        buf.save_to_file(path.string());
        EXPECT_FALSE(buf.is_modified());
    }

    {
        Buffer loaded = Buffer::from_file(path.string());
        EXPECT_EQ(loaded.to_string(), content);
    }

    std::filesystem::remove(path);
}

TEST(BufferTest, MarkSavedClearsModified) {
    Buffer buf("abc");
    buf.insert(0, "x");
    EXPECT_TRUE(buf.is_modified());
    buf.mark_saved();
    EXPECT_FALSE(buf.is_modified());
}
