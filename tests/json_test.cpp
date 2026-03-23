#include "core/json.h"

#include <gtest/gtest.h>

using namespace beresta;

// --- Tokenizer tests ---

TEST(JsonTokenizerTest, EmptyObject) {
    auto tokens = tokenize_json("{}");
    ASSERT_GE(tokens.size(), 2);
    EXPECT_EQ(tokens[0].kind, JsonTokenKind::Brace);
    EXPECT_EQ(tokens[1].kind, JsonTokenKind::Brace);
}

TEST(JsonTokenizerTest, SimpleKeyValue) {
    auto tokens = tokenize_json(R"({"name": "Alice"})");
    // Expect: { "name" : "Alice" }
    // Find the string tokens.
    JsonTokenKind kinds_found;
    bool found_key = false, found_value = false;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::StringKey) found_key = true;
        if (t.kind == JsonTokenKind::StringValue) found_value = true;
    }
    EXPECT_TRUE(found_key);
    EXPECT_TRUE(found_value);
}

TEST(JsonTokenizerTest, NumberToken) {
    auto tokens = tokenize_json(R"({"x": 42})");
    bool found_number = false;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::Number) {
            found_number = true;
            EXPECT_EQ(t.len, 2); // "42"
        }
    }
    EXPECT_TRUE(found_number);
}

TEST(JsonTokenizerTest, BoolAndNull) {
    auto tokens = tokenize_json(R"({"a": true, "b": false, "c": null})");
    int bool_null_count = 0;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::BoolNull) bool_null_count++;
    }
    EXPECT_EQ(bool_null_count, 3);
}

TEST(JsonTokenizerTest, NestedObject) {
    auto tokens = tokenize_json(R"({"a": {"b": 1}})");
    int brace_count = 0;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::Brace) brace_count++;
    }
    EXPECT_EQ(brace_count, 4); // 2 opening + 2 closing
}

TEST(JsonTokenizerTest, Array) {
    auto tokens = tokenize_json(R"([1, 2, 3])");
    int bracket_count = 0;
    int number_count = 0;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::Bracket) bracket_count++;
        if (t.kind == JsonTokenKind::Number) number_count++;
    }
    EXPECT_EQ(bracket_count, 2);
    EXPECT_EQ(number_count, 3);
}

TEST(JsonTokenizerTest, StringsInArray) {
    auto tokens = tokenize_json(R"(["a", "b"])");
    int string_value_count = 0;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::StringValue) string_value_count++;
    }
    EXPECT_EQ(string_value_count, 2);
}

TEST(JsonTokenizerTest, EscapedString) {
    auto tokens = tokenize_json(R"({"key": "val\"ue"})");
    bool found_value = false;
    for (const auto& t : tokens) {
        if (t.kind == JsonTokenKind::StringValue) {
            found_value = true;
            // "val\"ue" = 10 chars including quotes
        }
    }
    EXPECT_TRUE(found_value);
}

// --- Validator tests ---

TEST(JsonValidatorTest, ValidObject) {
    EXPECT_EQ(validate_json(R"({"name": "Alice", "age": 30})"), "");
}

TEST(JsonValidatorTest, ValidArray) {
    EXPECT_EQ(validate_json(R"([1, 2, 3])"), "");
}

TEST(JsonValidatorTest, ValidNested) {
    EXPECT_EQ(validate_json(R"({"a": [1, {"b": true}], "c": null})"), "");
}

TEST(JsonValidatorTest, EmptyObject) {
    EXPECT_EQ(validate_json("{}"), "");
}

TEST(JsonValidatorTest, EmptyArray) {
    EXPECT_EQ(validate_json("[]"), "");
}

TEST(JsonValidatorTest, InvalidMissingColon) {
    EXPECT_NE(validate_json(R"({"key" "value"})"), "");
}

TEST(JsonValidatorTest, InvalidTrailingComma) {
    EXPECT_NE(validate_json(R"({"a": 1,})"), "");
}

TEST(JsonValidatorTest, InvalidEmpty) {
    EXPECT_NE(validate_json(""), "");
}

TEST(JsonValidatorTest, InvalidBareWord) {
    EXPECT_NE(validate_json("hello"), "");
}

TEST(JsonValidatorTest, ValidString) {
    EXPECT_EQ(validate_json(R"("hello")"), "");
}

TEST(JsonValidatorTest, ValidNumber) {
    EXPECT_EQ(validate_json("42"), "");
}

TEST(JsonValidatorTest, ValidNegativeFloat) {
    EXPECT_EQ(validate_json("-3.14"), "");
}

TEST(JsonValidatorTest, ValidScientific) {
    EXPECT_EQ(validate_json("1.5e10"), "");
}

// --- Formatter tests ---

TEST(JsonFormatterTest, FormatSimple) {
    std::string input = R"({"name":"Alice","age":30})";
    std::string result = format_json(input);
    EXPECT_NE(result, input); // should be different (formatted)
    EXPECT_TRUE(result.find('\n') != std::string::npos); // should have newlines
    EXPECT_TRUE(result.find("\"name\"") != std::string::npos);
}

TEST(JsonFormatterTest, FormatEmptyObject) {
    EXPECT_EQ(format_json("{}"), "{}");
}

TEST(JsonFormatterTest, FormatInvalidReturnsOriginal) {
    std::string invalid = "not json";
    EXPECT_EQ(format_json(invalid), invalid);
}

TEST(JsonFormatterTest, FormatArray) {
    std::string input = R"([1,2,3])";
    std::string result = format_json(input);
    EXPECT_TRUE(result.find('\n') != std::string::npos);
}

TEST(JsonFormatterTest, FormatNested) {
    std::string input = R"({"a":{"b":1}})";
    std::string result = format_json(input);
    // Should have indentation.
    EXPECT_TRUE(result.find("  ") != std::string::npos);
}

// --- File detection ---

TEST(JsonFileTest, DetectsJsonExtension) {
    EXPECT_TRUE(is_json_file("config.json"));
    EXPECT_TRUE(is_json_file("/path/to/.mcp.json"));
    EXPECT_TRUE(is_json_file("FILE.JSON"));
}

TEST(JsonFileTest, RejectsNonJson) {
    EXPECT_FALSE(is_json_file("file.txt"));
    EXPECT_FALSE(is_json_file("file.js"));
    EXPECT_FALSE(is_json_file("json")); // no dot
}
