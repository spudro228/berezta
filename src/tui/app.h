#pragma once

#include "core/document.h"
#include "core/json.h"
#include "core/search.h"
#include "tui/input.h"
#include "tui/prompt.h"

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace beresta {

/// A text region pinned by the user for later copying.
struct PinnedItem {
    Range range;            ///< Position in the buffer at pin time.
    std::string text;       ///< Snapshot of the text.
    bool highlight_valid;   ///< false after buffer edits (offsets stale).
};

/// Rows reserved for the status bar and message bar.
constexpr size_t kUiReservedRows = 2;

/// How long a status message stays visible.
constexpr auto kStatusMessageDuration = std::chrono::seconds(5);

/// The main application: ties together document, input, and rendering.
class App {
public:
    App();
    explicit App(const std::string& file_path);

    int run();

    // Search state.
    std::vector<Match> search_matches;
    std::string search_query;

    // JSON state.
    std::vector<JsonToken> json_tokens;
    bool is_json_ = false;

private:
    Document doc_;
    bool running_ = true;
    int exit_code_ = 0;
    InputMode mode_ = InputMode::Normal;
    std::optional<std::string> status_message_;
    std::chrono::steady_clock::time_point status_message_time_;
    bool json_tokens_dirty_ = true;
    bool center_mode_ = false;
    bool is_git_message_ = false;

    std::unique_ptr<Prompt> search_prompt_;
    std::unique_ptr<Prompt> replace_prompt_;
    bool replace_mode_ = false;

    void set_status_message(const std::string& msg);
    std::optional<std::string> active_status_message() const;

    void render();
    void dispatch(const CommandEvent& event);
    void handle_prompt_key(int ch);

    void handle_save();
    void handle_quit();
    void handle_cancel();

    // Search.
    void open_search();
    void open_replace();
    void update_search_matches();
    void find_next();
    void find_prev();
    void replace_one();
    void replace_all();
    void close_search();

    // JSON.
    void update_json_tokens();
    void handle_format_json();
    void handle_validate_json();

    // Clipboard.
    std::string clipboard_;
    void copy_to_system_clipboard(const std::string& text);
    std::string paste_from_system_clipboard();
    void handle_copy();
    void handle_cut();
    void handle_paste();

    // Pinned selections.
    std::vector<PinnedItem> pinned_items_;
    size_t pinned_list_cursor_ = 0;
    bool pinned_panel_visible_ = true;
    void handle_pin_selection();
    void handle_pinned_list_key(const CommandEvent& event);
    /// Adjust pinned ranges after a buffer edit in region [edit_from, edit_to)
    /// with byte `delta`. Ranges after the edit are shifted, ranges overlapping
    /// the edited region are invalidated.
    void adjust_pinned_ranges(size_t edit_from, size_t edit_to, long delta);
    void invalidate_pinned_highlights();
};

} // namespace beresta
