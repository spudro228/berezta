#include "tui/renderer.h"
#include "tui/app.h"
#include "tui/terminal.h"
#include "core/utf8_util.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <utf8.h>

namespace berezta {

namespace {

bool is_in_matches(size_t char_offset, const std::vector<Match>& matches) {
    for (const auto& m : matches) {
        if (char_offset >= m.pos && char_offset < m.pos + m.len) return true;
    }
    return false;
}

JsonTokenKind json_token_at(size_t char_offset, const std::vector<JsonToken>& tokens) {
    for (const auto& t : tokens) {
        if (char_offset >= t.start && char_offset < t.start + t.len) {
            return t.kind;
        }
        if (t.start > char_offset) break;
    }
    return JsonTokenKind::Whitespace;
}

void apply_json_fg(JsonTokenKind kind) {
    switch (kind) {
        case JsonTokenKind::StringKey:   term::color::fg_cyan(); break;
        case JsonTokenKind::StringValue: term::color::fg_green(); break;
        case JsonTokenKind::Number:      term::color::fg_yellow(); break;
        case JsonTokenKind::BoolNull:    term::color::fg_magenta(); break;
        case JsonTokenKind::Brace:
        case JsonTokenKind::Bracket:     term::color::fg_white(); break;
        case JsonTokenKind::Colon:
        case JsonTokenKind::Comma:       term::color::fg_grey(); break;
        case JsonTokenKind::Error:       term::color::fg_red(); break;
        default: break;
    }
}

} // anonymous namespace

void render_document(const Document& doc, size_t editor_height,
                     const std::vector<Match>& search_matches,
                     const std::vector<JsonToken>& json_tokens,
                     size_t left_padding,
                     const std::vector<PinnedItem>* pinned_items,
                     bool git_mode) {
    auto cursor_pos = doc.selection.primary().cursor();
    auto [cursor_line, _] = doc.buffer.char_to_pos(cursor_pos);
    const auto& selections = doc.selection.ranges();
    auto [term_width, term_height] = term::get_terminal_size();
    size_t total_left = left_padding + kLineNumberWidth;
    size_t content_width = term_width > total_left ? term_width - total_left : 1;
    bool has_json = !json_tokens.empty();

    for (size_t screen_row = 0; screen_row < editor_height; ++screen_row) {
        size_t doc_line = doc.viewport.scroll_line + screen_row;
        term::move_cursor(0, screen_row);

        // Left padding (center mode).
        if (left_padding > 0) {
            term::write(std::string(left_padding, ' '));
        }

        if (doc_line < doc.buffer.line_count()) {
            // --- Line number ---
            if (doc_line == cursor_line) {
                term::color::fg_yellow();
            } else {
                term::color::fg_grey();
            }
            std::fprintf(stdout, "%*zu ", static_cast<int>(kLineNumberWidth - 1), doc_line + 1);
            term::color::fg_default();

            // --- Git commit message highlighting ---
            bool git_comment = false;
            bool git_subject_overflow = false;
            if (git_mode) {
                const std::string& gl = doc.buffer.line(doc_line);
                if (!gl.empty() && gl[0] == '#') {
                    git_comment = true;
                } else if (doc_line == 0 && ustr::display_width(gl) > 50) {
                    git_subject_overflow = true;
                }
            }

            // --- Line content (UTF-8 aware) ---
            const std::string& line_text = doc.buffer.line(doc_line);
            size_t line_start_char = doc.buffer.pos_to_char(doc_line, 0);
            size_t scroll_col = doc.viewport.scroll_col;

            size_t display_col = 0;
            size_t chars_printed = 0;
            auto it = line_text.begin();
            auto end = line_text.end();
            size_t byte_idx = 0;

            while (it != end && chars_printed < content_width) {
                size_t cp_byte_start = byte_idx;
                uint32_t cp = utf8::next(it, end);
                size_t cp_byte_end = static_cast<size_t>(it - line_text.begin());
                int cp_width = ustr::codepoint_width(cp);
                if (cp_width < 0) cp_width = 0;

                byte_idx = cp_byte_end;

                if (display_col + static_cast<size_t>(cp_width) <= scroll_col) {
                    display_col += static_cast<size_t>(cp_width);
                    continue;
                }
                if (display_col < scroll_col) {
                    display_col += static_cast<size_t>(cp_width);
                    continue;
                }
                if (chars_printed + static_cast<size_t>(cp_width) > content_width) {
                    break;
                }

                size_t char_offset = line_start_char + cp_byte_start;

                bool in_selection = false;
                for (const auto& r : selections) {
                    if (!r.is_empty() && char_offset >= r.from() && char_offset < r.to()) {
                        in_selection = true;
                        break;
                    }
                }

                bool in_pinned = false;
                if (pinned_items) {
                    for (const auto& item : *pinned_items) {
                        if (item.highlight_valid && char_offset >= item.range.from()
                            && char_offset < item.range.to()) {
                            in_pinned = true;
                            break;
                        }
                    }
                }

                bool in_match = !search_matches.empty() && is_in_matches(char_offset, search_matches);

                if (in_selection) {
                    term::color::bg_blue();
                } else if (in_pinned) {
                    term::set_bg(0, 80, 0);
                } else if (in_match) {
                    term::set_bg(80, 80, 0);
                }

                if (has_json) {
                    JsonTokenKind kind = json_token_at(char_offset, json_tokens);
                    apply_json_fg(kind);
                } else if (git_comment) {
                    term::color::fg_grey();
                } else if (git_subject_overflow && display_col >= 50) {
                    term::color::fg_red();
                }

                if (cp == '\t') {
                    term::write("    ");
                    chars_printed += 4;
                } else {
                    std::string cp_str(line_text.begin() + cp_byte_start,
                                       line_text.begin() + cp_byte_end);
                    term::write(cp_str);
                    chars_printed += static_cast<size_t>(cp_width);
                }

                display_col += static_cast<size_t>(cp_width);

                if (in_selection || in_pinned || in_match) {
                    term::color::bg_default();
                }
                if (has_json || git_comment || git_subject_overflow) {
                    term::color::fg_default();
                }
            }

            if (chars_printed < content_width) {
                term::write(std::string(content_width - chars_printed, ' '));
            }
        } else {
            term::color::fg_grey();
            std::fprintf(stdout, "%*s ", static_cast<int>(kLineNumberWidth - 1), "~");
            term::color::fg_default();
            term::write(std::string(content_width, ' '));
        }
    }
}

void position_terminal_cursor(const Document& doc, size_t left_padding) {
    auto cursor_pos = doc.selection.primary().cursor();
    auto [cursor_line, cursor_col_bytes] = doc.buffer.char_to_pos(cursor_pos);

    size_t cursor_display_col = doc.buffer.byte_to_display_col(cursor_line, cursor_col_bytes);

    size_t screen_row = cursor_line - doc.viewport.scroll_line;
    size_t screen_col = left_padding + kLineNumberWidth + cursor_display_col - doc.viewport.scroll_col;

    term::move_cursor(screen_col, screen_row);
    term::show_cursor();
}

void render_pinned_panel(const std::vector<PinnedItem>& items,
                         size_t cursor, bool focused,
                         size_t start_col, size_t panel_w, size_t editor_h) {
    if (items.empty() || panel_w < 4) return;

    size_t inner = panel_w - 1; // 1 col for left border

    // Scroll logic: figure out which items are visible.
    // Reserve 2 rows for header and 1 for footer hints when focused.
    size_t header_rows = 2;
    size_t footer_rows = focused ? 1 : 0;
    size_t avail = editor_h > header_rows + footer_rows ? editor_h - header_rows - footer_rows : 0;
    size_t scroll_offset = 0;
    if (focused && cursor >= avail && avail > 0) {
        scroll_offset = cursor - avail + 1;
    }

    for (size_t screen_row = 0; screen_row < editor_h; screen_row++) {
        term::move_cursor(start_col, screen_row);

        // Left border (separator from editor).
        term::color::fg_grey();
        term::write(u8"│");
        term::color::fg_default();

        if (screen_row == 0) {
            // Title row.
            term::color::fg_yellow();
            std::string title = u8" [" + std::to_string(items.size()) + u8"]";
            if (title.size() > inner) title = title.substr(0, inner);
            term::write(title);
            term::color::fg_default();
            if (title.size() < inner) term::write(std::string(inner - title.size(), ' '));
            continue;
        }

        if (screen_row == 1) {
            // Separator.
            term::color::fg_grey();
            for (size_t i = 0; i < inner; i++) term::write(u8"─");
            term::color::fg_default();
            continue;
        }

        // Footer hint (last row, only when focused).
        if (focused && screen_row == editor_h - 1) {
            term::color::fg_grey();
            std::string hint = u8" \u21b5:cp x:clr";
            if (hint.size() > inner) hint = hint.substr(0, inner);
            term::write(hint);
            term::color::fg_default();
            if (hint.size() < inner) term::write(std::string(inner - hint.size(), ' '));
            continue;
        }

        // Item rows.
        size_t item_slot = screen_row - header_rows;
        size_t item_idx = scroll_offset + item_slot;

        if (item_idx < items.size()) {
            bool is_active = focused && (item_idx == cursor);

            if (is_active) {
                term::color::bg_blue();
                term::color::fg_white();
            } else {
                term::color::fg_default();
            }

            // Format: " N. text "
            std::string num = " " + std::to_string(item_idx + 1) + ".";
            std::string text = items[item_idx].text;
            for (char& c : text) {
                if (c == '\n' || c == '\r') c = ' ';
            }

            size_t max_text = inner > num.size() + 2 ? inner - num.size() - 2 : 0;
            if (ustr::display_width(text) > max_text) {
                size_t target = max_text > 2 ? max_text - 2 : 0; // room for ".."
                // UTF-8-safe truncation: remove whole codepoints from the end.
                while (ustr::display_width(text) > target && !text.empty()) {
                    size_t prev = ustr::prev_codepoint(text, text.size());
                    text.erase(prev);
                }
                text += "..";
            }

            std::string line = num + " " + text;
            term::write(line);
            size_t lw = ustr::display_width(line);
            if (lw < inner) term::write(std::string(inner - lw, ' '));

            if (is_active) {
                term::reset_colors();
            }
        } else {
            // Empty row.
            term::write(std::string(inner, ' '));
        }
    }
}

} // namespace berezta
