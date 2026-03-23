#pragma once

#include "core/document.h"

#include <optional>
#include <string>

namespace beresta {

/// Render the status bar and optional message at the bottom of the terminal.
void render_status_bar(
    const Document& doc,
    size_t terminal_height,
    const std::optional<std::string>& message);

} // namespace beresta
