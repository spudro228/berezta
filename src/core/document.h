#pragma once

#include "core/buffer.h"
#include "core/history.h"
#include "core/selection.h"

#include <string>

namespace beresta {

/// Visible area of the document within the terminal.
struct Viewport {
    size_t scroll_line = 0;
    size_t scroll_col = 0;
    size_t height = 24;
    size_t width = 80;
};

/// Lines to keep visible above/below the cursor when scrolling.
constexpr size_t kScrollMargin = 3;

/// A document ties together a text buffer, cursor state, viewport, and history.
class Document {
public:
    Document(size_t viewport_width, size_t viewport_height);

    /// Open a file.
    static Document open(const std::string& path, size_t vp_width, size_t vp_height);

    /// Save to the associated file path.
    void save();

    /// Display title for the status bar.
    std::string title() const;

    /// Ensure the primary cursor is within the viewport.
    void ensure_cursor_visible();

    /// Resize the viewport.
    void resize_viewport(size_t width, size_t height);

    // --- Cursor movement ---
    void move_left();
    void move_right();
    void move_up();
    void move_down();
    void move_line_start();
    void move_line_end();
    void move_doc_start();
    void move_doc_end();
    void page_up();
    void page_down();
    void move_word_left();
    void move_word_right();

    // --- Selection (shift+arrow) ---
    void select_left();
    void select_right();
    void select_up();
    void select_down();
    void select_word_left();
    void select_word_right();
    void select_all();

    // --- Text editing (works with all cursors) ---
    void insert_char(char ch);
    void insert_text(const std::string& text);
    void insert_newline();
    void delete_backward();
    void delete_forward();
    void delete_line();

    // --- Multi-cursor ---
    void add_cursor_above();
    void add_cursor_below();
    /// Select the next occurrence of the word under cursor / current selection (Ctrl-D).
    void select_next_occurrence();
    /// Remove the last added cursor.
    void deselect_last_cursor();

    // --- Undo/Redo ---
    bool undo();
    bool redo();

    Buffer buffer;
    Selection selection;
    Viewport viewport;
    History history;
    std::string file_path;

private:
    /// Apply text insertion at all cursor positions. Handles selections.
    void apply_insert(const std::string& text, bool mergeable = false);

    /// Save buffer+selection state before an edit, apply the edit, record history.
    void record_edit(const std::string& buf_before, const Selection& sel_before,
                     bool mergeable = false);

    /// Get the word under the cursor at the given position.
    std::string word_at(size_t pos) const;

    static bool is_word_char(char c);
};

} // namespace beresta
