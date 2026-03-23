#pragma once

#include <string>

namespace beresta {

/// Every action the editor can perform.
///
/// Defined in core (not TUI) so command dispatch is testable
/// without terminal dependencies.
enum class Command {
    // --- Text input ---
    InsertChar,    // carries a char payload via CommandEvent (ASCII)
    InsertText,    // carries a UTF-8 string payload via CommandEvent
    InsertNewline,
    DeleteBackward,
    DeleteForward,

    // --- Cursor movement ---
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveWordLeft,
    MoveWordRight,
    MoveLineStart,
    MoveLineEnd,
    MoveDocStart,
    MoveDocEnd,
    PageUp,
    PageDown,

    // --- Selection ---
    SelectLeft,
    SelectRight,
    SelectUp,
    SelectDown,
    SelectAll,

    // --- Multi-cursor ---
    AddCursorAbove,
    AddCursorBelow,
    SelectNextOccurrence,
    DeselectLastCursor,

    // --- Editing ---
    Undo,
    Redo,
    Cut,
    Copy,
    Paste,

    // --- Search ---
    OpenSearch,
    OpenReplace,
    FindNext,
    FindPrev,
    ReplaceOne,
    ReplaceAll,

    // --- File ---
    Save,
    SaveAs,
    Quit,

    // --- JSON ---
    FormatJson,
    ValidateJson,

    // --- Pinned selections ---
    PinSelection,
    ShowPinnedList,
    ClearPinned,
    TogglePinnedPanel,

    // --- UI ---
    OpenCommandPalette,
    Cancel,
    ShowHelp,
    ToggleCenterMode,
};

/// A command together with optional payload (e.g., the character to insert).
struct CommandEvent {
    Command command;
    char ch = '\0';        // used for InsertChar (ASCII)
    std::string text;      // used for InsertText (UTF-8)
};

} // namespace beresta
