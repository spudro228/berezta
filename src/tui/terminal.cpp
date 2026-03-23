#include "tui/terminal.h"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace beresta::term {

namespace {
    struct termios original_termios;
    bool raw_mode_enabled = false;
    bool kitty_active = false;
} // anonymous namespace

void enable_raw_mode() {
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
        throw std::runtime_error("Failed to get terminal attributes");
    }

    struct termios raw = original_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1; // 100ms timeout

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
        throw std::runtime_error("Failed to set terminal to raw mode");
    }
    raw_mode_enabled = true;

    // Use fully-buffered stdout with a large buffer so all writes
    // accumulate until flush() is called, eliminating flicker.
    static char stdout_buf[1 << 16]; // 64 KB
    std::setvbuf(stdout, stdout_buf, _IOFBF, sizeof(stdout_buf));
}

void disable_raw_mode() {
    if (raw_mode_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
        raw_mode_enabled = false;
    }
}

void enter_alternate_screen() {
    std::fputs("\x1b[?1049h", stdout);
}

void leave_alternate_screen() {
    std::fputs("\x1b[?1049l", stdout);
}

void clear_screen() {
    std::fputs("\x1b[2J", stdout);
}

void hide_cursor() {
    std::fputs("\x1b[?25l", stdout);
}

void show_cursor() {
    std::fputs("\x1b[?25h", stdout);
}

void move_cursor(size_t col, size_t row) {
    // ANSI escape uses 1-based coordinates.
    std::fprintf(stdout, "\x1b[%zu;%zuH", row + 1, col + 1);
}

std::pair<size_t, size_t> get_terminal_size() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return {80, 24}; // fallback
    }
    return {static_cast<size_t>(ws.ws_col), static_cast<size_t>(ws.ws_row)};
}

void flush() {
    std::fflush(stdout);
}

void begin_sync() {
    // DEC mode 2026: synchronized output. Terminal holds display
    // updates until end_sync(). Supported by most modern terminals;
    // silently ignored by those that don't support it.
    std::fputs("\x1b[?2026h", stdout);
}

void end_sync() {
    std::fputs("\x1b[?2026l", stdout);
    std::fflush(stdout);
}

// --- Colors ---

void set_fg(int r, int g, int b) {
    std::fprintf(stdout, "\x1b[38;2;%d;%d;%dm", r, g, b);
}

void set_bg(int r, int g, int b) {
    std::fprintf(stdout, "\x1b[48;2;%d;%d;%dm", r, g, b);
}

void set_fg_default() {
    std::fputs("\x1b[39m", stdout);
}

void set_bg_default() {
    std::fputs("\x1b[49m", stdout);
}

void reset_colors() {
    std::fputs("\x1b[0m", stdout);
}

namespace color {
    void fg_white()   { std::fputs("\x1b[37m", stdout); }
    void fg_black()   { std::fputs("\x1b[30m", stdout); }
    void fg_grey()    { std::fputs("\x1b[90m", stdout); }
    void fg_yellow()  { std::fputs("\x1b[33m", stdout); }
    void fg_cyan()    { std::fputs("\x1b[36m", stdout); }
    void fg_green()   { std::fputs("\x1b[32m", stdout); }
    void fg_magenta() { std::fputs("\x1b[35m", stdout); }
    void fg_red()     { std::fputs("\x1b[31m", stdout); }
    void bg_white()   { std::fputs("\x1b[47m", stdout); }
    void bg_blue()    { std::fputs("\x1b[44m", stdout); }
    void bg_default() { std::fputs("\x1b[49m", stdout); }
    void fg_default() { std::fputs("\x1b[39m", stdout); }
} // namespace color

void write(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout);
}

void write(const char* s) {
    std::fputs(s, stdout);
}

void write(char c) {
    std::fputc(c, stdout);
}

bool try_enable_kitty_keyboard() {
    // Query current Kitty keyboard protocol mode: CSI ? u
    std::fputs("\x1b[?u", stdout);
    std::fflush(stdout);

    // Temporarily increase read timeout for the detection response.
    struct termios current;
    tcgetattr(STDIN_FILENO, &current);
    struct termios tmp = current;
    tmp.c_cc[VTIME] = 2; // 200ms
    tmp.c_cc[VMIN] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &tmp);

    char buf[32];
    ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf) - 1);

    // Restore original timeout.
    tcsetattr(STDIN_FILENO, TCSANOW, &current);

    if (n <= 0) {
        kitty_active = false;
        return false;
    }

    // Expect response: ESC [ ? <digits> u
    buf[n] = '\0';
    bool supported = false;
    if (n >= 4 && buf[0] == '\x1b' && buf[1] == '[' && buf[2] == '?') {
        for (ssize_t i = 3; i < n; i++) {
            if (buf[i] == 'u') {
                supported = true;
                break;
            }
        }
    }

    if (supported) {
        // Enable with flags: 1 (disambiguate) | 4 (report alternate keys) = 5.
        // Uses progressive enhancement (push onto stack).
        std::fputs("\x1b[>5u", stdout);
        std::fflush(stdout);
        kitty_active = true;
    }

    return kitty_active;
}

void disable_kitty_keyboard() {
    if (kitty_active) {
        // Pop the mode we pushed.
        std::fputs("\x1b[<u", stdout);
        std::fflush(stdout);
        kitty_active = false;
    }
}

bool kitty_keyboard_active() {
    return kitty_active;
}

} // namespace beresta::term
