#pragma once

#include <cstddef>
#include <string>
#include <utility>

namespace berezta::term {

/// Enter raw mode (disable canonical input, echo, etc.).
void enable_raw_mode();

/// Restore the original terminal settings.
void disable_raw_mode();

/// Switch to the alternate screen buffer.
void enter_alternate_screen();

/// Return to the normal screen buffer.
void leave_alternate_screen();

/// Clear the entire terminal screen.
void clear_screen();

/// Hide the terminal cursor.
void hide_cursor();

/// Show the terminal cursor.
void show_cursor();

/// Move the cursor to (col, row), 0-based.
void move_cursor(size_t col, size_t row);

/// Get the terminal dimensions (columns, rows).
std::pair<size_t, size_t> get_terminal_size();

/// Flush stdout.
void flush();

/// Begin a synchronized output batch (reduces flicker).
/// Terminal buffers all output until end_sync().
void begin_sync();

/// End a synchronized output batch and display the result.
void end_sync();

/// Try to enable Kitty keyboard protocol for layout-independent hotkeys.
/// Returns true if the terminal supports it and it was enabled.
bool try_enable_kitty_keyboard();

/// Disable Kitty keyboard protocol (if it was enabled).
void disable_kitty_keyboard();

/// Whether Kitty keyboard protocol is currently active.
bool kitty_keyboard_active();

// --- ANSI color helpers ---

void set_fg(int r, int g, int b);
void set_bg(int r, int g, int b);
void set_fg_default();
void set_bg_default();
void reset_colors();

/// Named colors for convenience.
namespace color {
    void fg_white();
    void fg_black();
    void fg_grey();
    void fg_yellow();
    void fg_cyan();
    void fg_green();
    void fg_magenta();
    void fg_red();
    void bg_white();
    void bg_blue();
    void bg_default();
    void fg_default();
} // namespace color

/// Write a string to stdout (no newline).
void write(const std::string& s);
void write(const char* s);
void write(char c);

} // namespace berezta::term
