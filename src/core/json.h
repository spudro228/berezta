#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace berezta {

/// Token types for JSON syntax highlighting.
enum class JsonTokenKind {
    Brace,        // { }
    Bracket,      // [ ]
    Colon,
    Comma,
    StringKey,    // "key" before a colon
    StringValue,  // "value" after a colon or inside arrays
    Number,
    BoolNull,     // true, false, null
    Whitespace,
    Error,
};

/// A single token produced by the JSON lexer.
struct JsonToken {
    JsonTokenKind kind;
    size_t start;  // character offset within the full text
    size_t len;    // character length
};

/// Tokenize JSON text for syntax highlighting.
/// Returns a flat list of tokens covering the entire input.
std::vector<JsonToken> tokenize_json(const std::string& text);

/// Validate JSON using a simple parser. Returns empty string if valid,
/// or an error message if invalid.
std::string validate_json(const std::string& text);

/// Format (pretty-print) JSON. Returns formatted string,
/// or the original text if parsing fails.
std::string format_json(const std::string& text);

/// Check if a file path looks like a JSON file (by extension).
bool is_json_file(const std::string& path);

} // namespace berezta
