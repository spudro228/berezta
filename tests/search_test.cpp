#include "core/search.h"

#include <gtest/gtest.h>

using namespace berezta;

TEST(SearchTest, FindAllBasic) {
    auto matches = find_all("foo bar foo baz foo", "foo");
    EXPECT_EQ(matches.size(), 3);
    EXPECT_EQ(matches[0].pos, 0);
    EXPECT_EQ(matches[1].pos, 8);
    EXPECT_EQ(matches[2].pos, 16);
    for (const auto& m : matches) {
        EXPECT_EQ(m.len, 3);
    }
}

TEST(SearchTest, FindAllNoMatch) {
    auto matches = find_all("hello world", "xyz");
    EXPECT_TRUE(matches.empty());
}

TEST(SearchTest, FindAllEmptyQuery) {
    auto matches = find_all("hello", "");
    EXPECT_TRUE(matches.empty());
}

TEST(SearchTest, FindAllOverlapping) {
    auto matches = find_all("aaa", "aa");
    EXPECT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].pos, 0);
    EXPECT_EQ(matches[1].pos, 1);
}

TEST(SearchTest, FindNextBasic) {
    std::string text = "foo bar foo baz";
    EXPECT_EQ(find_next(text, "foo", 0), 0);
    EXPECT_EQ(find_next(text, "foo", 1), 8);
    EXPECT_EQ(find_next(text, "foo", 9), 0); // wraps around
}

TEST(SearchTest, FindNextNoWrap) {
    std::string text = "foo bar foo";
    EXPECT_EQ(find_next(text, "foo", 9, false), std::string::npos);
}

TEST(SearchTest, FindNextNotFound) {
    EXPECT_EQ(find_next("hello", "xyz", 0), std::string::npos);
}

TEST(SearchTest, FindPrevBasic) {
    std::string text = "foo bar foo baz";
    EXPECT_EQ(find_prev(text, "foo", 15), 8);
    EXPECT_EQ(find_prev(text, "foo", 8), 0);
}

TEST(SearchTest, FindPrevWraps) {
    std::string text = "foo bar foo";
    EXPECT_EQ(find_prev(text, "foo", 0), 8); // wraps to end
}

TEST(SearchTest, FindPrevNotFound) {
    EXPECT_EQ(find_prev("hello", "xyz", 5), std::string::npos);
}

TEST(SearchTest, FindAllSingleChar) {
    auto matches = find_all("abcabc", "a");
    EXPECT_EQ(matches.size(), 2);
    EXPECT_EQ(matches[0].pos, 0);
    EXPECT_EQ(matches[1].pos, 3);
}
