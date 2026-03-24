#pragma once

#include "core/command.h"

#include <optional>

namespace berezta {

/// The current input mode of the editor.
enum class InputMode {
    Normal,
    Search,
    Replace,
    CommandPalette,
    Help,
    PinnedList,
    QuitConfirm,
};

/// Read a single key event from stdin (blocking until available or timeout).
/// Returns std::nullopt on timeout or unrecognized input.
std::optional<CommandEvent> read_key_event(InputMode mode);

} // namespace berezta
