#include "tui/help.h"
#include "tui/terminal.h"

#include <cstring>
#include <string>
#include <vector>

namespace beresta {

namespace {

struct HelpLine {
    const char* key;
    const char* desc;
};

struct HelpSection {
    const char* title;
    std::vector<HelpLine> lines;
};

// UTF-8 strings work fine with C++17 and modern compilers.
const std::vector<HelpSection> kSections = {
    {u8"Навигация", {
        {u8"Стрелки",          u8"Перемещение"},
        {u8"Alt+Left/Right",   u8"По словам"},
        {u8"Home / End",       u8"Начало / конец стр."},
        {u8"PageUp/Down",      u8"Прокрутка"},
    }},
    {u8"Выделение", {
        {u8"Shift+Стрелки",    u8"Расширить выделение"},
        {u8"Shift+Alt+←/→",   u8"Выделять по словам"},
        {u8"Ctrl+A",           u8"Выделить всё"},
    }},
    {u8"Мультикурсор", {
        {u8"Ctrl+W",           u8"Выделить слово/след."},
        {u8"Alt+Up/Down",      u8"Курсор выше/ниже"},
        {u8"Escape",           u8"Один курсор"},
        {u8"",                 u8""},
        {u8"Как:",             u8"Ctrl+D, Ctrl+D..."},
        {u8"",                 u8"затем печатай"},
    }},
    {u8"Редактирование", {
        {u8"Ctrl+C / Ctrl+V",  u8"Копировать / Вставить"},
        {u8"Ctrl+X",           u8"Вырезать"},
        {u8"Ctrl+D",           u8"Удалить строку/строки"},
        {u8"Ctrl+Z / Ctrl+Y",  u8"Отмена / Возврат"},
    }},
    {u8"Закрепление", {
        {u8"Ctrl+B",           u8"Закрепить выделение"},
        {u8"Ctrl+L",           u8"Фокус на панель"},
        {u8"F3",               u8"Показать/скрыть панель"},
    }},
    {u8"Поиск", {
        {u8"Ctrl+F",           u8"Найти"},
        {u8"Ctrl+H",           u8"Найти и заменить"},
    }},
    {u8"Файл", {
        {u8"Ctrl+S",           u8"Сохранить"},
        {u8"Ctrl+Q",           u8"Выход"},
    }},
    {u8"Вид", {
        {u8"F2",               u8"Центрирование текста"},
    }},
    {u8"Кодировки", {
        {u8"UTF-8",            u8"По умолчанию"},
        {u8"Latin-1",          u8"ISO 8859-1"},
        {u8"CP1251",           u8"Windows-1251"},
        {u8"",                 u8"Автоопределение"},
    }},
};

// Box-drawing (UTF-8).
static const char* kTL = u8"┌";
static const char* kTR = u8"┐";
static const char* kBL = u8"└";
static const char* kBR = u8"┘";
static const char* kH  = u8"─";
static const char* kV  = u8"│";
static const char* kML = u8"├";
static const char* kMR = u8"┤";

/// Popup width in display columns.
constexpr size_t kPopupCols = 44;
/// Key column display width.
constexpr size_t kKeyColWidth = 20;

/// Count the display width of a UTF-8 string (approximate: ASCII = 1, non-ASCII = 1 per codepoint).
size_t display_width(const char* s) {
    size_t w = 0;
    size_t i = 0;
    size_t len = std::strlen(s);
    while (i < len) {
        unsigned char c = s[i];
        if (c < 0x80) { w++; i++; }
        else if (c < 0xE0) { w++; i += 2; }
        else if (c < 0xF0) { w++; i += 3; }
        else { w++; i += 4; } // simplified: 1 column per codepoint
    }
    return w;
}

/// Write a horizontal border.
void draw_hborder(size_t col, size_t row, size_t cols,
                  const char* l, const char* fill, const char* r) {
    term::move_cursor(col, row);
    term::color::fg_cyan();
    term::write(l);
    for (size_t i = 0; i < cols - 2; ++i) term::write(fill);
    term::write(r);
    term::color::fg_default();
}

/// Write a row with vertical borders and padded content.
/// `inner_cols` is the usable display width between the borders.
void draw_row(size_t col, size_t row, size_t cols) {
    term::move_cursor(col, row);
    term::color::fg_cyan();
    term::write(kV);
    term::color::fg_default();
    term::write(std::string(cols - 2, ' '));
    term::color::fg_cyan();
    term::write(kV);
    term::color::fg_default();
}

/// Pad with spaces to fill `target` display columns after already writing `used` columns.
void pad_to(size_t used, size_t target) {
    if (used < target) {
        term::write(std::string(target - used, ' '));
    }
}

} // anonymous namespace

void render_help_screen(size_t terminal_width, size_t terminal_height) {
    size_t popup_cols = kPopupCols;
    if (popup_cols > terminal_width) popup_cols = terminal_width;

    // Content lines count.
    size_t content = 2; // title + separator
    for (const auto& sec : kSections) {
        content += 1 + sec.lines.size() + 1;
    }
    content += 1; // footer

    size_t popup_rows = content + 2; // +borders
    size_t margin_top = 1;
    // Right-align with 1-column margin.
    size_t start_col = terminal_width > popup_cols + 1 ? terminal_width - popup_cols - 1 : 0;
    size_t start_row = margin_top;

    if (popup_rows + start_row > terminal_height) {
        popup_rows = terminal_height > start_row ? terminal_height - start_row : 1;
    }

    size_t inner = popup_cols - 2; // display columns between borders
    size_t row = start_row;

    // Top border.
    draw_hborder(start_col, row++, popup_cols, kTL, kH, kTR);

    // Title.
    if (row < start_row + popup_rows - 1) {
        term::move_cursor(start_col, row);
        term::color::fg_cyan(); term::write(kV); term::color::fg_default();
        term::color::fg_yellow();
        const char* title = u8" beresta";
        size_t tw = display_width(title);
        term::write(title);
        term::color::fg_default();
        pad_to(tw, inner);
        term::color::fg_cyan(); term::write(kV); term::color::fg_default();
        row++;
    }

    // Separator.
    if (row < start_row + popup_rows - 1) {
        draw_hborder(start_col, row++, popup_cols, kML, kH, kMR);
    }

    for (const auto& sec : kSections) {
        if (row >= start_row + popup_rows - 1) break;

        // Section title.
        term::move_cursor(start_col, row);
        term::color::fg_cyan(); term::write(kV);
        term::write(" ");
        size_t tw = display_width(sec.title);
        term::write(sec.title);
        term::color::fg_default();
        pad_to(tw + 1, inner);
        term::color::fg_cyan(); term::write(kV); term::color::fg_default();
        row++;

        // Entries.
        for (const auto& entry : sec.lines) {
            if (row >= start_row + popup_rows - 1) break;
            term::move_cursor(start_col, row);
            term::color::fg_cyan(); term::write(kV); term::color::fg_default();
            term::write("  ");

            term::color::fg_green();
            size_t kw = display_width(entry.key);
            term::write(entry.key);
            term::color::fg_default();
            pad_to(kw, kKeyColWidth);

            size_t desc_space = inner - 2 - kKeyColWidth;
            size_t dw = display_width(entry.desc);
            term::write(entry.desc);
            pad_to(2 + kKeyColWidth + dw, inner);

            term::color::fg_cyan(); term::write(kV); term::color::fg_default();
            row++;
        }

        // Blank line between sections.
        if (row < start_row + popup_rows - 1) {
            draw_row(start_col, row++, popup_cols);
        }
    }

    // Fill remaining.
    while (row < start_row + popup_rows - 2) {
        draw_row(start_col, row++, popup_cols);
    }

    // Footer.
    if (row < start_row + popup_rows - 1) {
        term::move_cursor(start_col, row);
        term::color::fg_cyan(); term::write(kV); term::color::fg_default();
        term::color::fg_grey();
        const char* footer = u8" Esc/F1 — закрыть";
        size_t fw = display_width(footer);
        term::write(footer);
        term::color::fg_default();
        pad_to(fw, inner);
        term::color::fg_cyan(); term::write(kV); term::color::fg_default();
        row++;
    }

    // Bottom border.
    draw_hborder(start_col, row, popup_cols, kBL, kH, kBR);
}

} // namespace beresta
