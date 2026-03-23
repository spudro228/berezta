#include "core/document.h"
#include "core/utf8_util.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace beresta {

Document::Document(size_t viewport_width, size_t viewport_height)
    : selection(Selection::point(0)) {
    viewport.width = viewport_width;
    viewport.height = viewport_height;
}

Document Document::open(const std::string& path, size_t vp_width, size_t vp_height) {
    Document doc(vp_width, vp_height);
    doc.buffer = Buffer::from_file(path);
    doc.file_path = path;
    return doc;
}

void Document::save() {
    if (file_path.empty()) {
        throw std::runtime_error("No file path set");
    }
    buffer.save_to_file(file_path);
}

std::string Document::title() const {
    if (file_path.empty()) {
        return "[untitled]";
    }
    return std::filesystem::path(file_path).filename().string();
}

void Document::ensure_cursor_visible() {
    auto cursor_pos = selection.primary().cursor();
    auto [cursor_line, cursor_col_bytes] = buffer.char_to_pos(cursor_pos);

    // Use display column for horizontal scrolling.
    size_t cursor_display_col = buffer.byte_to_display_col(cursor_line, cursor_col_bytes);

    if (cursor_line < viewport.scroll_line + kScrollMargin) {
        viewport.scroll_line = (cursor_line > kScrollMargin)
            ? cursor_line - kScrollMargin : 0;
    } else if (cursor_line + kScrollMargin >= viewport.scroll_line + viewport.height) {
        viewport.scroll_line = cursor_line + kScrollMargin + 1 - viewport.height;
    }

    if (cursor_display_col < viewport.scroll_col) {
        viewport.scroll_col = cursor_display_col;
    } else if (cursor_display_col >= viewport.scroll_col + viewport.width) {
        viewport.scroll_col = cursor_display_col - viewport.width + 2;
    }
}

void Document::resize_viewport(size_t width, size_t height) {
    viewport.width = width;
    viewport.height = height;
    ensure_cursor_visible();
}

// --- Cursor movement ---

void Document::move_left() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        if (!r.is_empty()) return Range::point(r.from());
        if (r.cursor() == 0) return Range::point(0);
        std::string text = buf.to_string();
        return Range::point(ustr::prev_codepoint(text, r.cursor()));
    });
    ensure_cursor_visible();
}

void Document::move_right() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        if (!r.is_empty()) return Range::point(r.to());
        size_t max_pos = buf.total_chars();
        if (r.cursor() >= max_pos) return Range::point(max_pos);
        std::string text = buf.to_string();
        return Range::point(ustr::next_codepoint(text, r.cursor()));
    });
    ensure_cursor_visible();
}

void Document::move_up() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.cursor());
        if (line == 0) return Range::point(0);
        // Convert byte col to display col, then find byte pos on prev line.
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(line - 1, display_col);
        return Range::point(buf.pos_to_char(line - 1, new_byte_col));
    });
    ensure_cursor_visible();
}

void Document::move_down() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.cursor());
        size_t last_line = buf.line_count() - 1;
        if (line >= last_line) return Range::point(buf.total_chars());
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(line + 1, display_col);
        return Range::point(buf.pos_to_char(line + 1, new_byte_col));
    });
    ensure_cursor_visible();
}

void Document::move_line_start() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, _] = buf.char_to_pos(r.cursor());
        return Range::point(buf.pos_to_char(line, 0));
    });
    ensure_cursor_visible();
}

void Document::move_line_end() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, _] = buf.char_to_pos(r.cursor());
        return Range::point(buf.pos_to_char(line, buf.line_width(line)));
    });
    ensure_cursor_visible();
}

void Document::move_doc_start() {
    selection = Selection::point(0);
    ensure_cursor_visible();
}

void Document::move_doc_end() {
    selection = Selection::point(buffer.total_chars());
    ensure_cursor_visible();
}

void Document::page_up() {
    size_t page = viewport.height > 1 ? viewport.height - 1 : 1;
    const Buffer& buf = buffer;
    selection = selection.transform([&buf, page](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.cursor());
        size_t new_line = line > page ? line - page : 0;
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(new_line, display_col);
        return Range::point(buf.pos_to_char(new_line, new_byte_col));
    });
    viewport.scroll_line = viewport.scroll_line > page ? viewport.scroll_line - page : 0;
    ensure_cursor_visible();
}

void Document::page_down() {
    size_t page = viewport.height > 1 ? viewport.height - 1 : 1;
    const Buffer& buf = buffer;
    size_t last_line = buf.line_count() - 1;
    selection = selection.transform([&buf, page, last_line](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.cursor());
        size_t new_line = std::min(line + page, last_line);
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(new_line, display_col);
        return Range::point(buf.pos_to_char(new_line, new_byte_col));
    });
    viewport.scroll_line = std::min(viewport.scroll_line + page, last_line);
    ensure_cursor_visible();
}

bool Document::is_word_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

void Document::move_word_left() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        std::string text = buf.to_string();
        size_t pos = r.cursor();
        if (pos == 0) return Range::point(0);
        // Step back by codepoint, but check word chars by looking at the byte
        // before the codepoint boundary (works for ASCII word chars).
        while (pos > 0 && !is_word_char(text[pos - 1])) {
            pos = ustr::prev_codepoint(text, pos);
        }
        while (pos > 0 && is_word_char(text[pos - 1])) {
            pos = ustr::prev_codepoint(text, pos);
        }
        return Range::point(pos);
    });
    ensure_cursor_visible();
}

void Document::move_word_right() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        std::string text = buf.to_string();
        size_t len = text.size();
        size_t pos = r.cursor();
        if (pos >= len) return Range::point(len);
        // Step forward by codepoint.
        while (pos < len && is_word_char(text[pos])) {
            pos = ustr::next_codepoint(text, pos);
        }
        while (pos < len && !is_word_char(text[pos])) {
            pos = ustr::next_codepoint(text, pos);
        }
        return Range::point(pos);
    });
    ensure_cursor_visible();
}

// --- Selection (shift+arrow) ---

void Document::select_left() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        if (r.head == 0) return Range(r.anchor, 0);
        std::string text = buf.to_string();
        return Range(r.anchor, ustr::prev_codepoint(text, r.head));
    });
    ensure_cursor_visible();
}

void Document::select_right() {
    const Buffer& buf = buffer;
    size_t max_pos = buf.total_chars();
    selection = selection.transform([&buf, max_pos](Range r) {
        if (r.head >= max_pos) return Range(r.anchor, max_pos);
        std::string text = buf.to_string();
        return Range(r.anchor, ustr::next_codepoint(text, r.head));
    });
    ensure_cursor_visible();
}

void Document::select_up() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.head);
        if (line == 0) return Range(r.anchor, 0);
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(line - 1, display_col);
        return Range(r.anchor, buf.pos_to_char(line - 1, new_byte_col));
    });
    ensure_cursor_visible();
}

void Document::select_down() {
    const Buffer& buf = buffer;
    selection = selection.transform([&buf](Range r) {
        auto [line, col_bytes] = buf.char_to_pos(r.head);
        size_t last_line = buf.line_count() - 1;
        if (line >= last_line) return Range(r.anchor, buf.total_chars());
        size_t display_col = buf.byte_to_display_col(line, col_bytes);
        size_t new_byte_col = buf.display_col_to_byte(line + 1, display_col);
        return Range(r.anchor, buf.pos_to_char(line + 1, new_byte_col));
    });
    ensure_cursor_visible();
}

void Document::select_all() {
    selection = Selection::single(0, buffer.total_chars());
    ensure_cursor_visible();
}

// --- Text editing (multi-cursor) ---

void Document::insert_char(char ch) {
    apply_insert(std::string(1, ch));
}

void Document::insert_text(const std::string& text) {
    apply_insert(text);
}

void Document::insert_newline() {
    apply_insert("\n");
}

void Document::delete_backward() {
    // If there's a selection, delete it.
    bool has_sel = false;
    for (const auto& r : selection.ranges()) {
        if (!r.is_empty()) { has_sel = true; break; }
    }
    if (has_sel) {
        apply_insert("");
        return;
    }

    std::string buf_before = buffer.to_string();
    Selection sel_before = selection;
    const auto& ranges = selection.ranges();

    struct Op { size_t from; size_t to; };
    std::vector<Op> ops;
    for (const auto& r : ranges) {
        size_t pos = r.cursor();
        if (pos == 0) continue;
        size_t del_from = ustr::prev_codepoint(buf_before, pos);
        ops.push_back({del_from, pos});
    }
    if (ops.empty()) return;

    for (int i = static_cast<int>(ops.size()) - 1; i >= 0; --i) {
        buffer.remove(ops[i].from, ops[i].to);
    }

    std::vector<Range> new_ranges;
    size_t cum = 0;
    for (size_t i = 0; i < ops.size(); ++i) {
        new_ranges.push_back(Range::point(ops[i].from - cum));
        cum += ops[i].to - ops[i].from;
    }
    selection = Selection::from_ranges(std::move(new_ranges), sel_before.primary_idx());
    record_edit(buf_before, sel_before);
    ensure_cursor_visible();
}

void Document::delete_forward() {
    bool has_sel = false;
    for (const auto& r : selection.ranges()) {
        if (!r.is_empty()) { has_sel = true; break; }
    }
    if (has_sel) {
        apply_insert("");
        return;
    }

    std::string buf_before = buffer.to_string();
    Selection sel_before = selection;
    const auto& ranges = selection.ranges();
    size_t total = buf_before.size();

    struct Op { size_t from; size_t to; };
    std::vector<Op> ops;
    for (const auto& r : ranges) {
        size_t pos = r.cursor();
        if (pos >= total) continue;
        size_t del_to = ustr::next_codepoint(buf_before, pos);
        ops.push_back({pos, del_to});
    }
    if (ops.empty()) return;

    for (int i = static_cast<int>(ops.size()) - 1; i >= 0; --i) {
        buffer.remove(ops[i].from, ops[i].to);
    }

    std::vector<Range> new_ranges;
    size_t cum = 0;
    for (size_t i = 0; i < ops.size(); ++i) {
        new_ranges.push_back(Range::point(ops[i].from - cum));
        cum += ops[i].to - ops[i].from;
    }
    selection = Selection::from_ranges(std::move(new_ranges), sel_before.primary_idx());
    record_edit(buf_before, sel_before);
    ensure_cursor_visible();
}

void Document::record_edit(const std::string& buf_before, const Selection& sel_before) {
    history.record(Edit{
        buf_before,
        buffer.to_string(),
        sel_before,
        selection,
    });
}

void Document::apply_insert(const std::string& text) {
    std::string buf_before = buffer.to_string();
    Selection sel_before = selection;
    const auto& ranges = selection.ranges();

    // Collect edit positions (sorted by from(), which they already are).
    struct Op { size_t from; size_t to; };
    std::vector<Op> ops;
    ops.reserve(ranges.size());
    for (const auto& r : ranges) {
        ops.push_back({r.from(), r.to()});
    }

    // Apply from back to front so positions stay valid.
    for (int i = static_cast<int>(ops.size()) - 1; i >= 0; --i) {
        buffer.replace(ops[i].from, ops[i].to, text);
    }

    // Compute new cursor positions after all edits.
    std::vector<Range> new_ranges;
    size_t cumulative_shift = 0;
    for (size_t i = 0; i < ops.size(); ++i) {
        size_t removed_len = ops[i].to - ops[i].from;
        size_t new_pos = ops[i].from + cumulative_shift + text.size();
        new_ranges.push_back(Range::point(new_pos));
        cumulative_shift += text.size() - removed_len;
    }
    selection = Selection::from_ranges(std::move(new_ranges), sel_before.primary_idx());

    record_edit(buf_before, sel_before);
    ensure_cursor_visible();
}

// --- Multi-cursor ---

void Document::add_cursor_above() {
    Range primary = selection.primary();
    auto [line, col_bytes] = buffer.char_to_pos(primary.cursor());
    if (line == 0) return;
    size_t display_col = buffer.byte_to_display_col(line, col_bytes);
    size_t new_byte_col = buffer.display_col_to_byte(line - 1, display_col);
    size_t new_pos = buffer.pos_to_char(line - 1, new_byte_col);
    selection = selection.push(Range::point(new_pos));
    ensure_cursor_visible();
}

void Document::add_cursor_below() {
    Range primary = selection.primary();
    auto [line, col_bytes] = buffer.char_to_pos(primary.cursor());
    size_t last_line = buffer.line_count() - 1;
    if (line >= last_line) return;
    size_t display_col = buffer.byte_to_display_col(line, col_bytes);
    size_t new_byte_col = buffer.display_col_to_byte(line + 1, display_col);
    size_t new_pos = buffer.pos_to_char(line + 1, new_byte_col);
    selection = selection.push(Range::point(new_pos));
    ensure_cursor_visible();
}

std::string Document::word_at(size_t pos) const {
    std::string text = buffer.to_string();
    if (pos >= text.size()) return "";

    size_t start = pos;
    size_t end = pos;

    // Expand to word boundaries.
    while (start > 0 && is_word_char(text[start - 1])) start--;
    while (end < text.size() && is_word_char(text[end])) end++;

    if (start == end) return "";
    return text.substr(start, end - start);
}

void Document::select_next_occurrence() {
    Range primary = selection.primary();

    // If cursor has no selection, first select the word under cursor.
    if (primary.is_empty()) {
        std::string word = word_at(primary.cursor());
        if (word.empty()) return;

        // Find the word boundaries around the cursor.
        std::string text = buffer.to_string();
        size_t pos = primary.cursor();
        size_t start = pos;
        while (start > 0 && is_word_char(text[start - 1])) start--;

        // Replace the cursor with a selection over the word.
        selection = Selection::single(start, start + word.size());
        // Now fall through to find the next occurrence.
    }

    // Determine what to search for from the primary selection.
    std::string search_text = buffer.slice(selection.primary().from(), selection.primary().to());
    if (search_text.empty()) return;

    // Search forward from the end of the last range.
    const auto& ranges = selection.ranges();
    size_t search_start = ranges.back().to();

    std::string full_text = buffer.to_string();
    size_t found = full_text.find(search_text, search_start);

    // Wrap around if not found after last range.
    if (found == std::string::npos) {
        found = full_text.find(search_text, 0);
    }

    if (found == std::string::npos) return;

    // Don't add if it overlaps with an existing range.
    Range new_range(found, found + search_text.size());
    for (const auto& r : ranges) {
        if (r.overlaps(new_range)) return;
    }

    selection = selection.push(new_range);
    ensure_cursor_visible();
}

void Document::deselect_last_cursor() {
    if (selection.is_single()) return;
    // Remove the last range (most recently added tends to be last after sort).
    selection = selection.remove(selection.len() - 1);
    ensure_cursor_visible();
}

// --- Undo/Redo ---

bool Document::undo() {
    Edit edit;
    if (!history.undo(edit)) return false;

    buffer = Buffer(edit.buffer_before);
    selection = edit.selection_before;
    ensure_cursor_visible();
    return true;
}

bool Document::redo() {
    Edit edit;
    if (!history.redo(edit)) return false;

    buffer = Buffer(edit.buffer_after);
    selection = edit.selection_after;
    ensure_cursor_visible();
    return true;
}

} // namespace beresta
