#include "core/json.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stack>

namespace berezta {

namespace {

/// Lexer state to track whether the next string is a key or value.
enum class LexContext {
    ObjectKey,   // expecting a key or closing brace
    ObjectColon, // just saw a key, expecting colon
    ObjectValue, // just saw colon, expecting value
    ArrayValue,  // inside an array
    TopLevel,    // top-level
};

bool is_number_start(char c) {
    return c == '-' || (c >= '0' && c <= '9');
}

bool is_number_char(char c) {
    return (c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
        || c == '+' || c == '-';
}

} // anonymous namespace

std::vector<JsonToken> tokenize_json(const std::string& text) {
    std::vector<JsonToken> tokens;
    std::stack<LexContext> context;
    context.push(LexContext::TopLevel);
    size_t i = 0;

    while (i < text.size()) {
        char c = text[i];

        // Whitespace.
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            size_t start = i;
            while (i < text.size() && (text[i] == ' ' || text[i] == '\t'
                   || text[i] == '\n' || text[i] == '\r')) {
                i++;
            }
            tokens.push_back({JsonTokenKind::Whitespace, start, i - start});
            continue;
        }

        // Opening brace.
        if (c == '{') {
            tokens.push_back({JsonTokenKind::Brace, i, 1});
            context.push(LexContext::ObjectKey);
            i++;
            continue;
        }

        // Closing brace.
        if (c == '}') {
            tokens.push_back({JsonTokenKind::Brace, i, 1});
            if (!context.empty() && (context.top() == LexContext::ObjectKey
                || context.top() == LexContext::ObjectValue
                || context.top() == LexContext::ObjectColon)) {
                context.pop();
            }
            i++;
            continue;
        }

        // Opening bracket.
        if (c == '[') {
            tokens.push_back({JsonTokenKind::Bracket, i, 1});
            context.push(LexContext::ArrayValue);
            i++;
            continue;
        }

        // Closing bracket.
        if (c == ']') {
            tokens.push_back({JsonTokenKind::Bracket, i, 1});
            if (!context.empty() && context.top() == LexContext::ArrayValue) {
                context.pop();
            }
            i++;
            continue;
        }

        // Colon.
        if (c == ':') {
            tokens.push_back({JsonTokenKind::Colon, i, 1});
            if (!context.empty() && context.top() == LexContext::ObjectColon) {
                context.top() = LexContext::ObjectValue;
            }
            i++;
            continue;
        }

        // Comma.
        if (c == ',') {
            tokens.push_back({JsonTokenKind::Comma, i, 1});
            if (!context.empty() && context.top() == LexContext::ObjectValue) {
                context.top() = LexContext::ObjectKey;
            }
            // ArrayValue stays ArrayValue after comma.
            i++;
            continue;
        }

        // String.
        if (c == '"') {
            size_t start = i;
            i++; // skip opening quote
            while (i < text.size() && text[i] != '"') {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    i++; // skip escaped char
                }
                i++;
            }
            if (i < text.size()) {
                i++; // skip closing quote
            }

            // Determine if key or value based on context.
            LexContext ctx = context.empty() ? LexContext::TopLevel : context.top();
            JsonTokenKind kind;
            if (ctx == LexContext::ObjectKey) {
                kind = JsonTokenKind::StringKey;
                context.top() = LexContext::ObjectColon;
            } else {
                kind = JsonTokenKind::StringValue;
                // After a value in object, next token should be comma or closing brace.
                // We don't change context here — comma handler does it.
            }
            tokens.push_back({kind, start, i - start});
            continue;
        }

        // Number.
        if (is_number_start(c)) {
            size_t start = i;
            i++;
            while (i < text.size() && is_number_char(text[i])) {
                i++;
            }
            tokens.push_back({JsonTokenKind::Number, start, i - start});
            continue;
        }

        // true, false, null.
        if (c == 't' || c == 'f' || c == 'n') {
            size_t start = i;
            if (text.compare(i, 4, "true") == 0) {
                tokens.push_back({JsonTokenKind::BoolNull, start, 4});
                i += 4;
                continue;
            }
            if (text.compare(i, 5, "false") == 0) {
                tokens.push_back({JsonTokenKind::BoolNull, start, 5});
                i += 5;
                continue;
            }
            if (text.compare(i, 4, "null") == 0) {
                tokens.push_back({JsonTokenKind::BoolNull, start, 4});
                i += 4;
                continue;
            }
            // Fall through to error.
        }

        // Unrecognized character.
        tokens.push_back({JsonTokenKind::Error, i, 1});
        i++;
    }

    return tokens;
}

// --- Simple JSON validator (recursive descent) ---

namespace {

class JsonValidator {
public:
    explicit JsonValidator(const std::string& text) : text_(text) {}

    std::string validate() {
        skip_whitespace();
        if (pos_ >= text_.size()) return "Empty input";
        std::string err = parse_value();
        if (!err.empty()) return err;
        skip_whitespace();
        if (pos_ < text_.size()) return error("Unexpected content after value");
        return "";
    }

private:
    const std::string& text_;
    size_t pos_ = 0;

    std::string error(const std::string& msg) const {
        return msg + " at position " + std::to_string(pos_);
    }

    void skip_whitespace() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            pos_++;
        }
    }

    char peek() const {
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    std::string parse_value() {
        skip_whitespace();
        char c = peek();
        if (c == '"') return parse_string();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') return parse_null();
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number();
        return error("Unexpected character '" + std::string(1, c) + "'");
    }

    std::string parse_string() {
        if (peek() != '"') return error("Expected '\"'");
        pos_++;
        while (pos_ < text_.size() && text_[pos_] != '"') {
            if (text_[pos_] == '\\') pos_++;
            pos_++;
        }
        if (pos_ >= text_.size()) return error("Unterminated string");
        pos_++; // closing quote
        return "";
    }

    std::string parse_object() {
        pos_++; // skip '{'
        skip_whitespace();
        if (peek() == '}') { pos_++; return ""; }
        while (true) {
            skip_whitespace();
            if (peek() != '"') return error("Expected string key");
            std::string err = parse_string();
            if (!err.empty()) return err;
            skip_whitespace();
            if (peek() != ':') return error("Expected ':'");
            pos_++;
            skip_whitespace();
            err = parse_value();
            if (!err.empty()) return err;
            skip_whitespace();
            if (peek() == '}') { pos_++; return ""; }
            if (peek() != ',') return error("Expected ',' or '}'");
            pos_++;
        }
    }

    std::string parse_array() {
        pos_++; // skip '['
        skip_whitespace();
        if (peek() == ']') { pos_++; return ""; }
        while (true) {
            skip_whitespace();
            std::string err = parse_value();
            if (!err.empty()) return err;
            skip_whitespace();
            if (peek() == ']') { pos_++; return ""; }
            if (peek() != ',') return error("Expected ',' or ']'");
            pos_++;
        }
    }

    std::string parse_bool() {
        if (text_.compare(pos_, 4, "true") == 0) { pos_ += 4; return ""; }
        if (text_.compare(pos_, 5, "false") == 0) { pos_ += 5; return ""; }
        return error("Invalid boolean");
    }

    std::string parse_null() {
        if (text_.compare(pos_, 4, "null") == 0) { pos_ += 4; return ""; }
        return error("Invalid null");
    }

    std::string parse_number() {
        size_t start = pos_;
        if (peek() == '-') pos_++;
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            return error("Invalid number");
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        if (pos_ < text_.size() && text_[pos_] == '.') {
            pos_++;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return error("Invalid number");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            pos_++;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) pos_++;
            if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                return error("Invalid number exponent");
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) pos_++;
        }
        if (pos_ == start) return error("Invalid number");
        return "";
    }
};

} // anonymous namespace

std::string validate_json(const std::string& text) {
    JsonValidator validator(text);
    return validator.validate();
}

// --- Simple JSON formatter (pretty-printer) ---

std::string format_json(const std::string& text) {
    // First validate.
    std::string err = validate_json(text);
    if (!err.empty()) return text; // return as-is if invalid

    std::string result;
    result.reserve(text.size() * 2);
    int indent = 0;
    bool in_string = false;
    constexpr int kIndentSize = 2;

    auto add_newline_indent = [&]() {
        result += '\n';
        for (int j = 0; j < indent * kIndentSize; ++j) {
            result += ' ';
        }
    };

    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];

        if (in_string) {
            result += c;
            if (c == '"') {
                in_string = false;
            } else if (c == '\\' && i + 1 < text.size()) {
                result += text[++i];
            }
            continue;
        }

        switch (c) {
            case '"':
                in_string = true;
                result += c;
                break;
            case '{':
            case '[':
                result += c;
                indent++;
                // Check if next non-whitespace is closing bracket.
                {
                    size_t j = i + 1;
                    while (j < text.size() && std::isspace(static_cast<unsigned char>(text[j]))) j++;
                    char closing = (c == '{') ? '}' : ']';
                    if (j < text.size() && text[j] == closing) {
                        // Empty object/array — don't add newline.
                        indent--;
                        result += closing;
                        i = j; // skip to closing bracket
                        break;
                    }
                }
                add_newline_indent();
                break;
            case '}':
            case ']':
                indent--;
                add_newline_indent();
                result += c;
                break;
            case ',':
                result += c;
                add_newline_indent();
                break;
            case ':':
                result += ": ";
                break;
            default:
                if (!std::isspace(static_cast<unsigned char>(c))) {
                    result += c;
                }
                break;
        }
    }

    return result;
}

bool is_json_file(const std::string& path) {
    if (path.size() < 5) return false;
    std::string ext = path.substr(path.size() - 5);
    // Convert to lowercase.
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return ext == ".json";
}

} // namespace berezta
