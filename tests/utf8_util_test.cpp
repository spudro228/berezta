#include "core/utf8_util.h"

#include <gtest/gtest.h>

using namespace beresta;

TEST(Utf8UtilTest, CodepointCountAscii) {
    EXPECT_EQ(ustr::codepoint_count("hello"), 5);
    EXPECT_EQ(ustr::codepoint_count(""), 0);
}

TEST(Utf8UtilTest, CodepointCountCyrillic) {
    // "Привет" = 6 codepoints, each 2 bytes in UTF-8
    EXPECT_EQ(ustr::codepoint_count(u8"\u041f\u0440\u0438\u0432\u0435\u0442"), 6);
}

TEST(Utf8UtilTest, CodepointCountMixed) {
    // "Hi Мир" = 6 codepoints
    EXPECT_EQ(ustr::codepoint_count(u8"Hi \u041c\u0438\u0440"), 6);
}

TEST(Utf8UtilTest, DisplayWidthAscii) {
    EXPECT_EQ(ustr::display_width("hello"), 5);
}

TEST(Utf8UtilTest, DisplayWidthCyrillic) {
    // Cyrillic characters are 1 column wide each.
    EXPECT_EQ(ustr::display_width(u8"\u041f\u0440\u0438\u0432\u0435\u0442"), 6);
}

TEST(Utf8UtilTest, DisplayWidthCJK) {
    // CJK ideograph U+4E16 (世) = 2 columns.
    EXPECT_EQ(ustr::display_width(u8"\u4e16"), 2);
    // "世界" = 4 columns.
    EXPECT_EQ(ustr::display_width(u8"\u4e16\u754c"), 4);
}

TEST(Utf8UtilTest, DisplayWidthMixed) {
    // "Hi世界" = 2 (ASCII) + 4 (CJK) = 6
    EXPECT_EQ(ustr::display_width(u8"Hi\u4e16\u754c"), 6);
}

TEST(Utf8UtilTest, NextCodepoint) {
    std::string s = u8"A\u041f"; // 'A' (1 byte) + 'П' (2 bytes)
    EXPECT_EQ(ustr::next_codepoint(s, 0), 1); // after 'A'
    EXPECT_EQ(ustr::next_codepoint(s, 1), 3); // after 'П'
    EXPECT_EQ(ustr::next_codepoint(s, 3), 3); // at end, stays
}

TEST(Utf8UtilTest, PrevCodepoint) {
    std::string s = u8"A\u041f"; // 'A' (1 byte) + 'П' (2 bytes)
    EXPECT_EQ(ustr::prev_codepoint(s, 3), 1); // before 'П'
    EXPECT_EQ(ustr::prev_codepoint(s, 1), 0); // before 'A'
    EXPECT_EQ(ustr::prev_codepoint(s, 0), 0); // at start, stays
}

TEST(Utf8UtilTest, CodepointToByteAndBack) {
    std::string s = u8"AB\u041f\u0440"; // A(1) B(1) П(2) р(2) = 6 bytes
    EXPECT_EQ(ustr::codepoint_to_byte(s, 0), 0);
    EXPECT_EQ(ustr::codepoint_to_byte(s, 1), 1);
    EXPECT_EQ(ustr::codepoint_to_byte(s, 2), 2); // start of П
    EXPECT_EQ(ustr::codepoint_to_byte(s, 3), 4); // start of р
    EXPECT_EQ(ustr::codepoint_to_byte(s, 4), 6); // end

    EXPECT_EQ(ustr::byte_to_codepoint(s, 0), 0);
    EXPECT_EQ(ustr::byte_to_codepoint(s, 2), 2);
    EXPECT_EQ(ustr::byte_to_codepoint(s, 4), 3);
    EXPECT_EQ(ustr::byte_to_codepoint(s, 6), 4);
}

TEST(Utf8UtilTest, SubstrCp) {
    std::string s = u8"Hello \u041c\u0438\u0440"; // "Hello Мир"
    EXPECT_EQ(ustr::substr_cp(s, 0, 5), "Hello");
    EXPECT_EQ(ustr::substr_cp(s, 6, 9), u8"\u041c\u0438\u0440");
}

TEST(Utf8UtilTest, IsValid) {
    EXPECT_TRUE(ustr::is_valid("hello"));
    EXPECT_TRUE(ustr::is_valid(u8"\u041f\u0440\u0438\u0432\u0435\u0442"));
    EXPECT_TRUE(ustr::is_valid(""));

    // Invalid: lone continuation byte.
    std::string invalid = "\x80\x80";
    EXPECT_FALSE(ustr::is_valid(invalid));
}

TEST(Utf8UtilTest, CodepointWidthBasic) {
    EXPECT_EQ(ustr::codepoint_width('A'), 1);
    EXPECT_EQ(ustr::codepoint_width(0x041F), 1); // П
    EXPECT_EQ(ustr::codepoint_width(0x4E16), 2); // 世
    EXPECT_EQ(ustr::codepoint_width(0x0300), 0); // combining grave accent
}
