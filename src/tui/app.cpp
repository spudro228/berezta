#include "tui/app.h"
#include "tui/help.h"
#include "tui/renderer.h"
#include "tui/status_bar.h"
#include "tui/terminal.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <unistd.h>

namespace beresta {

namespace {
bool is_git_message_file(const std::string& path) {
    std::string name = std::filesystem::path(path).filename().string();
    return name == "COMMIT_EDITMSG" || name == "MERGE_MSG" ||
           name == "TAG_EDITMSG" || name == "SQUASH_MSG" ||
           name == "git-rebase-todo" || name == "addp-hunk-edit.diff";
}
} // anonymous namespace

App::App()
    : doc_([]() {
        auto [w, h] = term::get_terminal_size();
        size_t editor_h = h > kUiReservedRows ? h - kUiReservedRows : 1;
        return Document(w, editor_h);
    }()) {}

App::App(const std::string& file_path)
    : doc_([]() {
        auto [w, h] = term::get_terminal_size();
        size_t editor_h = h > kUiReservedRows ? h - kUiReservedRows : 1;
        return Document(w, editor_h);
    }()) {
    doc_ = Document::open(file_path, doc_.viewport.width, doc_.viewport.height);
    is_json_ = is_json_file(file_path);
    is_git_message_ = is_git_message_file(file_path);
}

int App::run() {
    term::enable_raw_mode();
    term::enter_alternate_screen();
    term::clear_screen();
    term::hide_cursor();
    term::try_enable_kitty_keyboard();

    while (running_) {
        render();

        // Use read_key_event which handles escape sequences properly.
        auto event = read_key_event(mode_);
        if (!event.has_value()) continue;

        if (mode_ == InputMode::QuitConfirm) {
            // y = save & exit(0), n = exit(1), Esc = cancel
            if (event->command == Command::InsertChar) {
                char ch = event->ch;
                if (ch == 'y' || ch == 'Y') {
                    handle_save();
                    exit_code_ = 0;
                    running_ = false;
                } else if (ch == 'n' || ch == 'N') {
                    exit_code_ = 1;
                    running_ = false;
                }
            } else if (event->command == Command::Cancel) {
                mode_ = InputMode::Normal;
                set_status_message("");
            }
            continue;
        }

        if (mode_ == InputMode::Help) {
            // Any key closes help.
            if (event->command == Command::Cancel || event->command == Command::ShowHelp) {
                mode_ = InputMode::Normal;
            }
            // Ignore all other keys in help mode.
            continue;
        }

        if (mode_ == InputMode::PinnedList) {
            if (event->command == Command::Cancel || event->command == Command::ShowPinnedList) {
                mode_ = InputMode::Normal;
            } else {
                handle_pinned_list_key(*event);
            }
            continue;
        }

        if (mode_ == InputMode::Search || mode_ == InputMode::Replace) {
            // Prompt mode: most keys go to the prompt handler.
            if (event->command == Command::Cancel) {
                close_search();
            } else if (event->command == Command::ShowHelp) {
                mode_ = InputMode::Help;
            } else if (event->command == Command::InsertChar) {
                handle_prompt_key(event->ch);
            } else if (event->command == Command::InsertText) {
                // UTF-8 input in search prompt.
                if (search_prompt_) {
                    for (char ch : event->text) {
                        search_prompt_->insert_char(ch);
                    }
                    update_search_matches();
                }
            } else if (event->command == Command::DeleteBackward) {
                handle_prompt_key(127);
            } else if (event->command == Command::InsertNewline) {
                handle_prompt_key('\r');
            } else {
                // Pass navigation commands through to search.
                dispatch(*event);
            }
        } else {
            dispatch(*event);
        }
    }

    term::disable_kitty_keyboard();
    term::show_cursor();
    term::leave_alternate_screen();
    term::disable_raw_mode();
    return exit_code_;
}

void App::set_status_message(const std::string& msg) {
    status_message_ = msg;
    status_message_time_ = std::chrono::steady_clock::now();
}

std::optional<std::string> App::active_status_message() const {
    if (!status_message_.has_value()) return std::nullopt;
    auto elapsed = std::chrono::steady_clock::now() - status_message_time_;
    if (elapsed > kStatusMessageDuration) return std::nullopt;
    return status_message_;
}

void App::render() {
    term::begin_sync();
    auto [w, h] = term::get_terminal_size();
    size_t editor_h = h > kUiReservedRows ? h - kUiReservedRows : 1;

    // Right panel for pinned selections.
    constexpr size_t kPinnedPanelWidth = 32;
    bool show_panel = pinned_panel_visible_ && !pinned_items_.empty() && w > kPinnedPanelWidth + 30;
    size_t panel_w = show_panel ? kPinnedPanelWidth : 0;
    size_t editor_w = w - panel_w;

    // Center mode: calculate left padding to center a ~80-column text area.
    constexpr size_t kCenterTargetWidth = 80;
    size_t left_padding = 0;
    if (center_mode_ && editor_w > kCenterTargetWidth + kLineNumberWidth) {
        left_padding = (editor_w - kCenterTargetWidth - kLineNumberWidth) / 2;
    }

    size_t effective_width = center_mode_
        ? std::min(editor_w, kCenterTargetWidth + kLineNumberWidth + left_padding)
        : editor_w;
    doc_.resize_viewport(effective_width - left_padding, editor_h);

    term::hide_cursor();
    if (is_json_ && json_tokens_dirty_) {
        update_json_tokens();
    }
    render_document(doc_, editor_h, search_matches, json_tokens, left_padding,
                    pinned_items_.empty() ? nullptr : &pinned_items_,
                    is_git_message_);

    // Pinned selections panel (right column).
    if (show_panel) {
        bool focused = (mode_ == InputMode::PinnedList);
        render_pinned_panel(pinned_items_, pinned_list_cursor_, focused,
                            editor_w, panel_w, editor_h);
    }

    if (mode_ == InputMode::Help) {
        // Draw popup overlay on top of the document.
        render_help_screen(w, h);
        render_status_bar(doc_, h, active_status_message(), is_git_message_);
        term::hide_cursor();
        term::end_sync();
        return;
    }

    // Status bar or prompt.
    if (mode_ == InputMode::Search || mode_ == InputMode::Replace) {
        // Render search prompt instead of message bar.
        size_t status_row = h >= 2 ? h - 2 : 0;
        term::move_cursor(0, status_row);
        term::color::fg_black();
        term::color::bg_white();

        std::string left = " " + doc_.title() + (doc_.buffer.is_modified() ? " [+]" : "");
        size_t match_count = search_matches.size();
        std::string right = " " + std::to_string(match_count) + " matches ";
        size_t mid = w > left.size() + right.size() ? w - left.size() - right.size() : 0;
        term::write(left);
        term::write(std::string(mid, ' '));
        term::write(right);
        term::reset_colors();

        // Prompt line.
        size_t msg_row = h >= 1 ? h - 1 : 0;
        term::move_cursor(0, msg_row);
        Prompt* active_prompt = search_prompt_.get();
        if (replace_mode_ && replace_prompt_) {
            // Show both on one line: "Find: xxx | Replace: yyy"
            std::string prompt_text = search_prompt_->label() + search_prompt_->input();
            if (replace_prompt_) {
                prompt_text += " | " + replace_prompt_->label() + replace_prompt_->input();
            }
            if (prompt_text.size() < w) {
                prompt_text += std::string(w - prompt_text.size(), ' ');
            }
            term::write(prompt_text);
        } else if (search_prompt_) {
            std::string prompt_text = search_prompt_->label() + search_prompt_->input();
            if (prompt_text.size() < w) {
                prompt_text += std::string(w - prompt_text.size(), ' ');
            }
            term::write(prompt_text);
        }

        // Position cursor in the prompt.
        if (search_prompt_) {
            size_t prompt_cursor_col = search_prompt_->label().size() + search_prompt_->cursor_pos();
            term::move_cursor(prompt_cursor_col, msg_row);
        }
        term::show_cursor();
    } else {
        render_status_bar(doc_, h, active_status_message(), is_git_message_);
        position_terminal_cursor(doc_, left_padding);
    }

    term::end_sync();
}

void App::dispatch(const CommandEvent& event) {
    switch (event.command) {
        // --- Navigation (breaks undo merge) ---
        case Command::MoveLeft:      doc_.history.break_merge(); doc_.move_left(); break;
        case Command::MoveRight:     doc_.history.break_merge(); doc_.move_right(); break;
        case Command::MoveUp:        doc_.history.break_merge(); doc_.move_up(); break;
        case Command::MoveDown:      doc_.history.break_merge(); doc_.move_down(); break;
        case Command::MoveWordLeft:  doc_.history.break_merge(); doc_.move_word_left(); break;
        case Command::MoveWordRight: doc_.history.break_merge(); doc_.move_word_right(); break;
        case Command::MoveLineStart: doc_.history.break_merge(); doc_.move_line_start(); break;
        case Command::MoveLineEnd:   doc_.history.break_merge(); doc_.move_line_end(); break;
        case Command::MoveDocStart:  doc_.history.break_merge(); doc_.move_doc_start(); break;
        case Command::MoveDocEnd:    doc_.history.break_merge(); doc_.move_doc_end(); break;
        case Command::PageUp:        doc_.history.break_merge(); doc_.page_up(); break;
        case Command::PageDown:      doc_.history.break_merge(); doc_.page_down(); break;

        // --- Selection (breaks undo merge) ---
        case Command::SelectLeft:  doc_.history.break_merge(); doc_.select_left(); break;
        case Command::SelectRight: doc_.history.break_merge(); doc_.select_right(); break;
        case Command::SelectUp:    doc_.history.break_merge(); doc_.select_up(); break;
        case Command::SelectDown:      doc_.history.break_merge(); doc_.select_down(); break;
        case Command::SelectWordLeft:  doc_.history.break_merge(); doc_.select_word_left(); break;
        case Command::SelectWordRight: doc_.history.break_merge(); doc_.select_word_right(); break;
        case Command::SelectAll:       doc_.history.break_merge(); doc_.select_all(); break;

        // --- Editing ---
        case Command::Copy: handle_copy(); break;
        case Command::Cut:  handle_cut(); break;
        case Command::Paste: handle_paste(); break;
        case Command::DeleteLine: {
            size_t edit_from = doc_.buffer.pos_to_char(
                doc_.buffer.char_to_pos(doc_.selection.primary().from()).first, 0);
            size_t old_size = doc_.buffer.total_chars();
            doc_.delete_line();
            json_tokens_dirty_ = true;
            long delta = static_cast<long>(doc_.buffer.total_chars()) - static_cast<long>(old_size);
            adjust_pinned_ranges(edit_from, edit_from + static_cast<size_t>(-delta), delta);
            break;
        }

        // --- Text editing ---
        case Command::InsertChar:
        case Command::InsertText:
        case Command::InsertNewline:
        case Command::DeleteBackward:
        case Command::DeleteForward: {
            size_t edit_from = doc_.selection.primary().from();
            size_t edit_to = doc_.selection.primary().to();
            size_t old_size = doc_.buffer.total_chars();
            switch (event.command) {
                case Command::InsertChar:     doc_.insert_char(event.ch); break;
                case Command::InsertText:     doc_.insert_text(event.text); break;
                case Command::InsertNewline:  doc_.insert_newline(); break;
                case Command::DeleteBackward: doc_.delete_backward(); break;
                case Command::DeleteForward:  doc_.delete_forward(); break;
                default: break;
            }
            json_tokens_dirty_ = true;
            long delta = static_cast<long>(doc_.buffer.total_chars()) - static_cast<long>(old_size);
            adjust_pinned_ranges(edit_from, edit_to, delta);
            break;
        }

        // --- Multi-cursor ---
        case Command::AddCursorAbove:       doc_.add_cursor_above(); break;
        case Command::AddCursorBelow:       doc_.add_cursor_below(); break;
        case Command::SelectNextOccurrence: doc_.select_next_occurrence(); break;
        case Command::DeselectLastCursor:   doc_.deselect_last_cursor(); break;

        // --- Undo/Redo ---
        case Command::Undo:
            if (!doc_.undo()) set_status_message("Nothing to undo");
            json_tokens_dirty_ = true;
            invalidate_pinned_highlights();
            break;
        case Command::Redo:
            if (!doc_.redo()) set_status_message("Nothing to redo");
            json_tokens_dirty_ = true;
            invalidate_pinned_highlights();
            break;

        // --- Pinned selections ---
        case Command::PinSelection:  handle_pin_selection(); break;
        case Command::ShowPinnedList:
            if (pinned_items_.empty()) {
                set_status_message(u8"Нет закреплённых выделений");
            } else {
                mode_ = InputMode::PinnedList;
                pinned_list_cursor_ = 0;
            }
            break;
        case Command::ClearPinned:
            pinned_items_.clear();
            mode_ = InputMode::Normal;
            set_status_message(u8"Все закрепления удалены");
            break;
        case Command::TogglePinnedPanel:
            pinned_panel_visible_ = !pinned_panel_visible_;
            set_status_message(pinned_panel_visible_ ? u8"Панель закреплений: вкл" : u8"Панель закреплений: выкл");
            if (!pinned_panel_visible_ && mode_ == InputMode::PinnedList) {
                mode_ = InputMode::Normal;
            }
            break;

        // --- Search ---
        case Command::OpenSearch:  open_search(); break;
        case Command::OpenReplace: open_replace(); break;
        case Command::FindNext:    find_next(); break;
        case Command::FindPrev:    find_prev(); break;
        case Command::ReplaceOne:  replace_one(); break;
        case Command::ReplaceAll:  replace_all(); break;

        // --- JSON ---
        case Command::FormatJson:   handle_format_json(); break;
        case Command::ValidateJson: handle_validate_json(); break;

        // --- File ---
        case Command::Save: handle_save(); break;
        case Command::Quit: handle_quit(); break;

        // --- Help ---
        case Command::ShowHelp: mode_ = InputMode::Help; break;

        // --- Center mode ---
        case Command::ToggleCenterMode:
            center_mode_ = !center_mode_;
            set_status_message(center_mode_ ? u8"Центрирование: вкл" : u8"Центрирование: выкл");
            break;

        // --- Cancel ---
        case Command::Cancel: handle_cancel(); break;

        default:
            set_status_message("Not yet implemented");
            break;
    }
}

void App::handle_prompt_key(int ch) {
    // Enter — confirm.
    if (ch == '\r' || ch == '\n') {
        if (replace_mode_) {
            replace_all();
            close_search();
        } else {
            find_next();
        }
        return;
    }

    // Backspace.
    if (ch == 127 || ch == 8) {
        if (search_prompt_) {
            search_prompt_->delete_backward();
            update_search_matches();
        }
        return;
    }

    // Printable character — add to prompt and update search.
    if (ch >= 32 && ch < 127 && search_prompt_) {
        search_prompt_->insert_char(static_cast<char>(ch));
        update_search_matches();
        // Incremental search: jump to nearest match.
        if (!search_matches.empty()) {
            size_t cursor_pos = doc_.selection.primary().cursor();
            size_t found = beresta::find_next(
                doc_.buffer.to_string(), search_prompt_->input(), cursor_pos);
            if (found != std::string::npos) {
                doc_.selection = Selection::single(found, found + search_prompt_->input().size());
                doc_.ensure_cursor_visible();
            }
        }
    }
}

void App::handle_save() {
    // Validate JSON before saving (warning only).
    if (is_json_) {
        std::string err = validate_json(doc_.buffer.to_string());
        if (!err.empty()) {
            set_status_message("Warning: invalid JSON — " + err + ". Saved anyway.");
            try { doc_.save(); } catch (...) {}
            return;
        }
    }
    try {
        doc_.save();
        set_status_message("File saved.");
    } catch (const std::exception& e) {
        set_status_message(std::string("Save failed: ") + e.what());
    }
}

void App::handle_quit() {
    if (doc_.buffer.is_modified()) {
        mode_ = InputMode::QuitConfirm;
        set_status_message(u8"Есть несохранённые изменения. Сохранить? (y/n/Esc)");
    } else {
        exit_code_ = 0;
        running_ = false;
    }
}

void App::handle_cancel() {
    if (mode_ == InputMode::Search || mode_ == InputMode::Replace) {
        close_search();
    } else if (!doc_.selection.is_single()) {
        doc_.selection = doc_.selection.collapse_to_primary();
    }
}

void App::open_search() {
    mode_ = InputMode::Search;
    replace_mode_ = false;
    search_prompt_ = std::make_unique<Prompt>("Find: ");
    replace_prompt_.reset();

    // Pre-fill with selected text.
    Range primary = doc_.selection.primary();
    if (!primary.is_empty()) {
        search_prompt_->set_input(doc_.buffer.slice(primary.from(), primary.to()));
        update_search_matches();
    }
}

void App::open_replace() {
    mode_ = InputMode::Replace;
    replace_mode_ = true;
    search_prompt_ = std::make_unique<Prompt>("Find: ");
    replace_prompt_ = std::make_unique<Prompt>("Replace: ");

    Range primary = doc_.selection.primary();
    if (!primary.is_empty()) {
        search_prompt_->set_input(doc_.buffer.slice(primary.from(), primary.to()));
        update_search_matches();
    }
}

void App::update_search_matches() {
    search_query = search_prompt_ ? search_prompt_->input() : "";
    if (search_query.empty()) {
        search_matches.clear();
    } else {
        search_matches = beresta::find_all(doc_.buffer.to_string(), search_query);
    }
}

void App::find_next() {
    if (search_query.empty()) return;
    size_t start = doc_.selection.primary().to();
    size_t found = beresta::find_next(doc_.buffer.to_string(), search_query, start);
    if (found != std::string::npos) {
        doc_.selection = Selection::single(found, found + search_query.size());
        doc_.ensure_cursor_visible();
    } else {
        set_status_message("Not found");
    }
}

void App::find_prev() {
    if (search_query.empty()) return;
    size_t start = doc_.selection.primary().from();
    size_t found = beresta::find_prev(doc_.buffer.to_string(), search_query, start);
    if (found != std::string::npos) {
        doc_.selection = Selection::single(found, found + search_query.size());
        doc_.ensure_cursor_visible();
    } else {
        set_status_message("Not found");
    }
}

void App::replace_one() {
    if (search_query.empty() || !replace_prompt_) return;

    Range primary = doc_.selection.primary();
    std::string selected = doc_.buffer.slice(primary.from(), primary.to());
    if (selected != search_query) {
        // Not on a match — find next first.
        find_next();
        return;
    }

    std::string replacement = replace_prompt_->input();
    // Delete the selected match and insert replacement.
    doc_.delete_forward(); // deletes selection
    for (char ch : replacement) {
        doc_.insert_char(ch);
    }
    update_search_matches();
    find_next();
}

void App::replace_all() {
    if (search_query.empty() || !replace_prompt_) return;

    std::string replacement = replace_prompt_->input();
    std::string text = doc_.buffer.to_string();
    std::string buf_before = text;
    Selection sel_before = doc_.selection;

    // Replace all occurrences.
    size_t count = 0;
    size_t pos = 0;
    std::string result;
    while (pos < text.size()) {
        size_t found = text.find(search_query, pos);
        if (found == std::string::npos) {
            result += text.substr(pos);
            break;
        }
        result += text.substr(pos, found - pos);
        result += replacement;
        pos = found + search_query.size();
        count++;
    }

    if (count == 0) {
        set_status_message("No matches to replace");
        return;
    }

    doc_.buffer = Buffer(result);
    doc_.selection = Selection::point(0);
    doc_.history.record(Edit{buf_before, result, sel_before, doc_.selection});
    update_search_matches();
    set_status_message("Replaced " + std::to_string(count) + " occurrences");
}

void App::close_search() {
    mode_ = InputMode::Normal;
    search_prompt_.reset();
    replace_prompt_.reset();
    search_matches.clear();
    search_query.clear();
}

// --- JSON ---

void App::update_json_tokens() {
    if (is_json_) {
        json_tokens = tokenize_json(doc_.buffer.to_string());
    } else {
        json_tokens.clear();
    }
    json_tokens_dirty_ = false;
}

void App::handle_format_json() {
    if (!is_json_) {
        set_status_message("Not a JSON file");
        return;
    }
    std::string text = doc_.buffer.to_string();
    std::string formatted = format_json(text);
    if (formatted == text) {
        std::string err = validate_json(text);
        if (!err.empty()) {
            set_status_message("Cannot format: " + err);
        } else {
            set_status_message("Already formatted");
        }
        return;
    }

    std::string buf_before = text;
    Selection sel_before = doc_.selection;
    doc_.buffer = Buffer(formatted);
    doc_.selection = Selection::point(0);
    doc_.history.record(Edit{buf_before, formatted, sel_before, doc_.selection});
    json_tokens_dirty_ = true;
    set_status_message("JSON formatted");
}

void App::handle_validate_json() {
    if (!is_json_) {
        set_status_message("Not a JSON file");
        return;
    }
    std::string err = validate_json(doc_.buffer.to_string());
    if (err.empty()) {
        set_status_message("Valid JSON");
    } else {
        set_status_message("Invalid JSON: " + err);
    }
}

// --- Clipboard ---

void App::copy_to_system_clipboard(const std::string& text) {
#ifdef __APPLE__
    FILE* pipe = popen("pbcopy", "w");
#else
    FILE* pipe = popen("xclip -selection clipboard 2>/dev/null || xsel --clipboard --input 2>/dev/null", "w");
#endif
    if (pipe) {
        std::fwrite(text.data(), 1, text.size(), pipe);
        pclose(pipe);
    }
}

std::string App::paste_from_system_clipboard() {
#ifdef __APPLE__
    FILE* pipe = popen("pbpaste", "r");
#else
    FILE* pipe = popen("xclip -selection clipboard -o 2>/dev/null || xsel --clipboard --output 2>/dev/null", "r");
#endif
    if (!pipe) return clipboard_;
    std::string result;
    char buf[1024];
    while (std::fgets(buf, sizeof(buf), pipe)) {
        result += buf;
    }
    pclose(pipe);
    return result;
}

void App::handle_copy() {
    Range primary = doc_.selection.primary();
    if (primary.is_empty()) {
        set_status_message(u8"Нечего копировать");
        return;
    }
    clipboard_ = doc_.buffer.slice(primary.from(), primary.to());
    copy_to_system_clipboard(clipboard_);
    set_status_message(u8"Скопировано");
}

void App::handle_cut() {
    Range primary = doc_.selection.primary();
    if (primary.is_empty()) {
        set_status_message(u8"Нечего вырезать");
        return;
    }
    clipboard_ = doc_.buffer.slice(primary.from(), primary.to());
    copy_to_system_clipboard(clipboard_);
    size_t edit_from = primary.from();
    size_t edit_to = primary.to();
    size_t old_size = doc_.buffer.total_chars();
    doc_.delete_forward();
    json_tokens_dirty_ = true;
    long delta = static_cast<long>(doc_.buffer.total_chars()) - static_cast<long>(old_size);
    adjust_pinned_ranges(edit_from, edit_to, delta);
}

void App::handle_paste() {
    std::string text = paste_from_system_clipboard();
    if (text.empty() && !clipboard_.empty()) text = clipboard_;
    if (text.empty()) {
        set_status_message(u8"Буфер обмена пуст");
        return;
    }
    size_t edit_from = doc_.selection.primary().from();
    size_t edit_to = doc_.selection.primary().to();
    size_t old_size = doc_.buffer.total_chars();
    doc_.insert_text(text);
    json_tokens_dirty_ = true;
    long delta = static_cast<long>(doc_.buffer.total_chars()) - static_cast<long>(old_size);
    adjust_pinned_ranges(edit_from, edit_to, delta);
}

// --- Pinned selections ---

void App::handle_pin_selection() {
    Range primary = doc_.selection.primary();
    if (primary.is_empty()) {
        set_status_message(u8"Выделите текст для закрепления (Shift+стрелки)");
        return;
    }

    // Remove any existing pinned items that overlap with the new selection.
    pinned_items_.erase(
        std::remove_if(pinned_items_.begin(), pinned_items_.end(),
            [&](const PinnedItem& item) {
                if (!item.highlight_valid) return false;
                return item.range.overlaps(primary);
            }),
        pinned_items_.end());

    std::string text = doc_.buffer.slice(primary.from(), primary.to());
    pinned_items_.insert(pinned_items_.begin(), {primary, text, true});

    // Truncate text for status message.
    std::string preview = text.size() > 30 ? text.substr(0, 30) + "..." : text;
    // Replace newlines for display.
    for (char& c : preview) {
        if (c == '\n' || c == '\r') c = ' ';
    }
    set_status_message(u8"Закреплено: \"" + preview + "\"");
}

void App::handle_pinned_list_key(const CommandEvent& event) {
    if (pinned_items_.empty()) {
        mode_ = InputMode::Normal;
        return;
    }

    switch (event.command) {
        case Command::MoveUp:
            if (pinned_list_cursor_ > 0) pinned_list_cursor_--;
            break;
        case Command::MoveDown:
            if (pinned_list_cursor_ + 1 < pinned_items_.size()) pinned_list_cursor_++;
            break;
        case Command::Copy: {
            // Enter: copy selected item.
            const auto& item = pinned_items_[pinned_list_cursor_];
            clipboard_ = item.text;
            copy_to_system_clipboard(clipboard_);
            set_status_message(u8"Скопировано в буфер обмена");
            mode_ = InputMode::Normal;
            break;
        }
        case Command::DeleteForward: {
            // Delete: remove item.
            pinned_items_.erase(pinned_items_.begin() + static_cast<long>(pinned_list_cursor_));
            if (pinned_items_.empty()) {
                mode_ = InputMode::Normal;
                set_status_message(u8"Все закрепления удалены");
            } else if (pinned_list_cursor_ >= pinned_items_.size()) {
                pinned_list_cursor_ = pinned_items_.size() - 1;
            }
            break;
        }
        case Command::SelectAll: {
            // 'a': copy all items joined by newlines.
            std::string all;
            for (size_t i = 0; i < pinned_items_.size(); i++) {
                if (i > 0) all += '\n';
                all += pinned_items_[i].text;
            }
            clipboard_ = all;
            copy_to_system_clipboard(clipboard_);
            set_status_message(u8"Все элементы скопированы");
            mode_ = InputMode::Normal;
            break;
        }
        case Command::ClearPinned:
            pinned_items_.clear();
            mode_ = InputMode::Normal;
            set_status_message(u8"Все закрепления удалены");
            break;
        case Command::TogglePinnedPanel:
            pinned_panel_visible_ = !pinned_panel_visible_;
            set_status_message(pinned_panel_visible_ ? u8"Панель закреплений: вкл" : u8"Панель закреплений: выкл");
            if (!pinned_panel_visible_) mode_ = InputMode::Normal;
            break;
        default:
            break;
    }
}

void App::adjust_pinned_ranges(size_t edit_from, size_t edit_to, long delta) {
    if (delta == 0 && edit_from == edit_to) return;

    for (auto& item : pinned_items_) {
        if (!item.highlight_valid) continue;

        size_t from = item.range.from();
        size_t to = item.range.to();

        // Pinned range is entirely before the edit — no change.
        if (to <= edit_from) continue;

        // Pinned range overlaps with the edited region — invalidate.
        if (from < edit_to && to > edit_from) {
            item.highlight_valid = false;
            continue;
        }

        // Pinned range is entirely after the edited region — shift.
        long new_anchor = static_cast<long>(item.range.anchor) + delta;
        long new_head = static_cast<long>(item.range.head) + delta;
        if (new_anchor < 0 || new_head < 0) {
            item.highlight_valid = false;
        } else {
            item.range.anchor = static_cast<size_t>(new_anchor);
            item.range.head = static_cast<size_t>(new_head);
        }
    }
}

void App::invalidate_pinned_highlights() {
    for (auto& item : pinned_items_) {
        item.highlight_valid = false;
    }
}

} // namespace beresta
