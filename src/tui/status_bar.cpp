#include "tui/status_bar.h"
#include "tui/terminal.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace beresta {

static const std::string kHelpHint = " F1 Help  F2 Center ";

void render_status_bar(
    const Document& doc,
    size_t terminal_height,
    const std::optional<std::string>& message)
{
    auto [term_width, _] = term::get_terminal_size();

    // --- Status bar (second-to-last row) ---
    std::string modified_marker = doc.buffer.is_modified() ? " [+]" : "";
    std::string left = " " + doc.title() + modified_marker;

    auto cursor_pos = doc.selection.primary().cursor();
    auto [line, col_bytes] = doc.buffer.char_to_pos(cursor_pos);
    // Show display column (1-based) instead of byte offset.
    size_t display_col = doc.buffer.byte_to_display_col(line, col_bytes);

    std::string encoding = doc.buffer.codec().name();
    std::string right;
    if (doc.selection.len() > 1) {
        right = " " + encoding + " | "
              + std::to_string(line + 1) + ":" + std::to_string(display_col + 1)
              + " (" + std::to_string(doc.selection.len()) + " cursors) ";
    } else {
        right = " " + encoding + " | "
              + std::to_string(line + 1) + ":" + std::to_string(display_col + 1) + " ";
    }

    size_t middle_width = (term_width > left.size() + right.size())
        ? term_width - left.size() - right.size()
        : 0;

    size_t status_row = terminal_height >= 2 ? terminal_height - 2 : 0;
    term::move_cursor(0, status_row);
    term::color::fg_black();
    term::color::bg_white();
    term::write(left);
    term::write(std::string(middle_width, ' '));
    term::write(right);
    term::reset_colors();

    // --- Message bar (last row): [F1 Help] message ---
    size_t msg_row = terminal_height >= 1 ? terminal_height - 1 : 0;
    term::move_cursor(0, msg_row);

    // Help hint -- bright, on the left.
    term::color::fg_black();
    term::set_bg(0, 180, 220); // bright cyan background
    term::write(kHelpHint);
    term::reset_colors();

    // Message after the hint.
    std::string msg = message.value_or("");
    size_t remaining = term_width > kHelpHint.size() ? term_width - kHelpHint.size() : 0;
    if (!msg.empty()) {
        std::string padded = " " + msg;
        if (padded.size() > remaining) padded = padded.substr(0, remaining);
        term::write(padded);
        if (padded.size() < remaining) {
            term::write(std::string(remaining - padded.size(), ' '));
        }
    } else {
        term::write(std::string(remaining, ' '));
    }
}

} // namespace beresta
