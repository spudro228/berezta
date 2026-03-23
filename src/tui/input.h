#pragma once

#include "core/command.h"

#include <optional>

namespace beresta {

/// The current input mode of the editor.
enum class InputMode {
    Normal,
    Search,
    Replace,
    CommandPalette,
    Help,
    PinnedList,
};

/// Read a single key event from stdin (blocking until available or timeout).
/// Returns std::nullopt on timeout or unrecognized input.
std::optional<CommandEvent> read_key_event(InputMode mode);

} // namespace beresta
