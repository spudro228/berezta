#pragma once

#include "core/document.h"
#include "core/json.h"
#include "core/search.h"

#include <cstddef>
#include <string>
#include <vector>

namespace beresta {

struct PinnedItem;  // forward declaration (defined in app.h)

/// Width of the line number gutter (digits + separator space).
constexpr size_t kLineNumberWidth = 5;

/// Render the document content to the terminal.
/// `left_padding` adds empty columns on the left (for center mode).
void render_document(const Document& doc, size_t editor_height,
                     const std::vector<Match>& search_matches = {},
                     const std::vector<JsonToken>& json_tokens = {},
                     size_t left_padding = 0,
                     const std::vector<PinnedItem>* pinned_items = nullptr);

/// Position the terminal cursor at the primary cursor's screen location.
/// `left_padding` must match the value passed to render_document.
void position_terminal_cursor(const Document& doc, size_t left_padding = 0);

/// Render the pinned selections panel as a right-side column.
/// `start_col` is the first column of the panel, `panel_w` its width.
void render_pinned_panel(const std::vector<PinnedItem>& items,
                         size_t cursor, bool focused,
                         size_t start_col, size_t panel_w, size_t editor_h);

} // namespace beresta
