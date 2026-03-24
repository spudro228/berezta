#pragma once

#include <cstddef>
#include <string>

namespace berezta {

/// State of the interactive prompt bar (search, replace, etc.).
class Prompt {
public:
    explicit Prompt(const std::string& label) : label_(label) {}

    const std::string& label() const { return label_; }
    const std::string& input() const { return input_; }
    size_t cursor_pos() const { return cursor_; }

    void insert_char(char ch);
    void delete_backward();
    void delete_forward();
    void move_left();
    void move_right();
    void move_home();
    void move_end();
    void clear();

    /// Set the input content (e.g., to pre-fill from selection).
    void set_input(const std::string& text);

private:
    std::string label_;
    std::string input_;
    size_t cursor_ = 0;
};

} // namespace berezta
