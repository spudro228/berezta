#include "tui/input.h"

#include <cstdio>
#include <string>
#include <unistd.h>

namespace beresta {

namespace {

/// Read a single byte from stdin. Returns -1 on EOF/timeout.
int read_byte() {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) return -1;
    return static_cast<unsigned char>(c);
}

/// Parse an integer from `s` starting at `pos`, advancing `pos` past the digits.
int parse_int(const std::string& s, size_t& pos) {
    int result = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        result = result * 10 + (s[pos] - '0');
        pos++;
    }
    return result;
}

/// Parse a CSI modifier+key for modified arrow sequences.
///
/// Modifier codes (char digit):
///   '2' = Shift,  '3' = Alt,  '4' = Shift+Alt,  '5' = Ctrl
std::optional<CommandEvent> parse_modified_arrow(char mod, int key) {
    switch (mod) {
        case '3': // Alt
            switch (key) {
                case 'A': return CommandEvent{Command::AddCursorAbove};
                case 'B': return CommandEvent{Command::AddCursorBelow};
                case 'C': return CommandEvent{Command::MoveWordRight};
                case 'D': return CommandEvent{Command::MoveWordLeft};
            }
            break;
        case '5': // Ctrl (fallback for Linux)
            switch (key) {
                case 'A': return CommandEvent{Command::AddCursorAbove};
                case 'B': return CommandEvent{Command::AddCursorBelow};
                case 'C': return CommandEvent{Command::MoveWordRight};
                case 'D': return CommandEvent{Command::MoveWordLeft};
            }
            break;
        case '2': // Shift
            switch (key) {
                case 'A': return CommandEvent{Command::SelectUp};
                case 'B': return CommandEvent{Command::SelectDown};
                case 'C': return CommandEvent{Command::SelectRight};
                case 'D': return CommandEvent{Command::SelectLeft};
            }
            break;
        case '4': // Shift+Alt
            switch (key) {
                case 'C': return CommandEvent{Command::SelectWordRight};
                case 'D': return CommandEvent{Command::SelectWordLeft};
            }
            break;
        case '6': // Shift+Ctrl
            switch (key) {
                case 'C': return CommandEvent{Command::SelectWordRight};
                case 'D': return CommandEvent{Command::SelectWordLeft};
            }
            break;
    }
    return std::nullopt;
}

/// Handle legacy CSI sequences (non-Kitty): arrows, PgUp/PgDn, Delete, F-keys,
/// modified arrows.
std::optional<CommandEvent> parse_csi_legacy(const std::string& params, int terminator) {
    // Simple keys with no parameters.
    if (params.empty()) {
        switch (terminator) {
            case 'A': return CommandEvent{Command::MoveUp};
            case 'B': return CommandEvent{Command::MoveDown};
            case 'C': return CommandEvent{Command::MoveRight};
            case 'D': return CommandEvent{Command::MoveLeft};
            case 'H': return CommandEvent{Command::MoveLineStart};
            case 'F': return CommandEvent{Command::MoveLineEnd};
        }
        return std::nullopt;
    }

    // CSI N ~ sequences.
    if (terminator == '~') {
        if (params == "3")  return CommandEvent{Command::DeleteForward};
        if (params == "5")  return CommandEvent{Command::PageUp};
        if (params == "6")  return CommandEvent{Command::PageDown};
        if (params == "11") return CommandEvent{Command::ShowHelp};          // F1
        if (params == "12") return CommandEvent{Command::ToggleCenterMode}; // F2
        if (params == "13") return CommandEvent{Command::TogglePinnedPanel}; // F3
        return std::nullopt;
    }

    // Modified arrows: CSI 1;{mod} {A/B/C/D}
    if (terminator >= 'A' && terminator <= 'D') {
        auto semi = params.find(';');
        if (semi != std::string::npos && semi + 1 < params.size()) {
            return parse_modified_arrow(params[semi + 1], terminator);
        }
    }

    return std::nullopt;
}

/// Map a ЙЦУКЕН Cyrillic codepoint to its QWERTY Latin equivalent.
/// Used as a fallback when the terminal doesn't report base_layout_key.
/// Returns the Latin letter, or 0 if the codepoint is not a Cyrillic key.
int cyrillic_to_latin(int cp) {
    switch (cp) {
        // lowercase
        case 0x0439: return 'q'; // й
        case 0x0446: return 'w'; // ц
        case 0x0443: return 'e'; // у
        case 0x043A: return 'r'; // к
        case 0x0435: return 't'; // е
        case 0x043D: return 'y'; // н
        case 0x0433: return 'u'; // г
        case 0x0448: return 'i'; // ш
        case 0x0449: return 'o'; // щ
        case 0x0437: return 'p'; // з
        case 0x0444: return 'a'; // ф
        case 0x044B: return 's'; // ы
        case 0x0432: return 'd'; // в
        case 0x0430: return 'f'; // а
        case 0x043F: return 'g'; // п
        case 0x0440: return 'h'; // р
        case 0x043E: return 'j'; // о
        case 0x043B: return 'k'; // л
        case 0x0434: return 'l'; // д
        case 0x044F: return 'z'; // я
        case 0x0447: return 'x'; // ч
        case 0x0441: return 'c'; // с
        case 0x043C: return 'v'; // м
        case 0x0438: return 'b'; // и
        case 0x0442: return 'n'; // т
        case 0x044C: return 'm'; // ь
        // uppercase
        case 0x0419: return 'q'; // Й
        case 0x0426: return 'w'; // Ц
        case 0x0423: return 'e'; // У
        case 0x041A: return 'r'; // К
        case 0x0415: return 't'; // Е
        case 0x041D: return 'y'; // Н
        case 0x0413: return 'u'; // Г
        case 0x0428: return 'i'; // Ш
        case 0x0429: return 'o'; // Щ
        case 0x0417: return 'p'; // З
        case 0x0424: return 'a'; // Ф
        case 0x042B: return 's'; // Ы
        case 0x0412: return 'd'; // В
        case 0x0410: return 'f'; // А
        case 0x041F: return 'g'; // П
        case 0x0420: return 'h'; // Р
        case 0x041E: return 'j'; // О
        case 0x041B: return 'k'; // Л
        case 0x0414: return 'l'; // Д
        case 0x042F: return 'z'; // Я
        case 0x0427: return 'x'; // Ч
        case 0x0421: return 'c'; // С
        case 0x041C: return 'v'; // М
        case 0x0418: return 'b'; // И
        case 0x0422: return 'n'; // Т
        case 0x042C: return 'm'; // Ь
        default: return 0;
    }
}

/// Handle Kitty keyboard protocol CSI u sequences.
///
/// Format: CSI key[:shifted[:base_layout]] [; modifiers[:event_type]] u
///
/// When a base_layout_key is present (flag 4 — report alternate keys),
/// it gives the US-layout key for the physical key pressed, regardless of
/// the active keyboard layout.  We use it to resolve hotkeys.
///
/// When base_layout_key is absent (terminal doesn't support flag 4),
/// we fall back to a ЙЦУКЕН→QWERTY mapping for Cyrillic codepoints.
std::optional<CommandEvent> parse_csi_u(const std::string& params) {
    size_t pos = 0;

    int key_code = parse_int(params, pos);
    int base_layout_key = 0;

    // Optional shifted_key and base_layout_key (colon-separated).
    if (pos < params.size() && params[pos] == ':') {
        pos++; // skip ':'
        /* int shifted_key = */ parse_int(params, pos);
        if (pos < params.size() && params[pos] == ':') {
            pos++;
            base_layout_key = parse_int(params, pos);
        }
    }

    // Modifiers (semicolon-separated).
    int modifiers = 0;
    int event_type = 1; // 1 = press (default)
    if (pos < params.size() && params[pos] == ';') {
        pos++;
        modifiers = parse_int(params, pos);
        if (pos < params.size() && params[pos] == ':') {
            pos++;
            event_type = parse_int(params, pos);
        }
    }

    // Ignore release events.
    if (event_type == 3) return std::nullopt;

    // Use physical (base layout) key when available.
    int effective_key = (base_layout_key > 0) ? base_layout_key : key_code;

    // Decode modifier bits (Kitty uses 1-indexed: value - 1 = bitfield).
    int mod_bits = (modifiers > 0) ? modifiers - 1 : 0;
    bool shift = mod_bits & 1;
    bool alt   = mod_bits & 2;
    bool ctrl  = mod_bits & 4;

    // Fallback: if base_layout_key wasn't reported and a modifier is active,
    // try mapping Cyrillic (ЙЦУКЕН) to Latin (QWERTY).
    if (base_layout_key == 0 && (ctrl || alt)) {
        int mapped = cyrillic_to_latin(effective_key);
        if (mapped > 0) effective_key = mapped;
    }

    // --- Ctrl+key ---
    if (ctrl && !alt) {
        switch (effective_key) {
            case 'a': return CommandEvent{Command::SelectAll};
            case 'b': return CommandEvent{Command::PinSelection};
            case 'c': return CommandEvent{Command::Copy};
            case 'd': return CommandEvent{Command::DeleteLine};
            case 'f': return CommandEvent{Command::OpenSearch};
            case 'h': return CommandEvent{Command::OpenReplace};
            case 'l': return CommandEvent{Command::ShowPinnedList};
            case 'w': return CommandEvent{Command::SelectNextOccurrence};
            case 'p': return CommandEvent{Command::OpenCommandPalette};
            case 'q': return CommandEvent{Command::Quit};
            case 's': return CommandEvent{Command::Save};
            case 'v': return CommandEvent{Command::Paste};
            case 'x': return CommandEvent{Command::Cut};
            case 'y': return CommandEvent{Command::Redo};
            case 'z': return CommandEvent{Command::Undo};
        }
    }

    // --- Shift+Alt+key (select by word) ---
    if (shift && alt && !ctrl) {
        switch (effective_key) {
            case 'b': return CommandEvent{Command::SelectWordLeft};
            case 'f': return CommandEvent{Command::SelectWordRight};
        }
    }

    // --- Alt+key ---
    if (alt && !ctrl) {
        switch (effective_key) {
            case 'b': return CommandEvent{Command::MoveWordLeft};
            case 'f': return CommandEvent{Command::MoveWordRight};
        }
    }

    // --- Unmodified special keys (disambiguated via Kitty protocol) ---
    if (!ctrl && !alt) {
        switch (effective_key) {
            case 13:  return CommandEvent{Command::InsertNewline};          // Enter
            case 9:   return CommandEvent{Command::InsertChar, '\t'};      // Tab
            case 27:  return CommandEvent{Command::Cancel};                // Escape
            case 127: return CommandEvent{Command::DeleteBackward};        // Backspace
        }
    }

    return std::nullopt;
}

/// Read the full CSI parameter bytes and terminator after ESC [ has been consumed.
/// Returns the parsed command, or nullopt.
std::optional<CommandEvent> read_csi_sequence() {
    std::string params;
    while (true) {
        int b = read_byte();
        if (b == -1) return CommandEvent{Command::Cancel};
        // Bytes 0x40–0x7E are CSI terminators.
        if (b >= 0x40 && b <= 0x7E) {
            if (b == 'u') return parse_csi_u(params);
            return parse_csi_legacy(params, b);
        }
        params += static_cast<char>(b);
    }
}

/// Try to read an escape sequence (called after the initial ESC byte).
std::optional<CommandEvent> read_escape_sequence() {
    int seq1 = read_byte();
    if (seq1 == -1) {
        return CommandEvent{Command::Cancel}; // bare Escape
    }

    // --- SS3 sequences: ESC O ... ---
    if (seq1 == 'O') {
        int seq2 = read_byte();
        if (seq2 == 'P') return CommandEvent{Command::ShowHelp};          // F1
        if (seq2 == 'Q') return CommandEvent{Command::ToggleCenterMode}; // F2
        if (seq2 == 'R') return CommandEvent{Command::TogglePinnedPanel}; // F3
        return std::nullopt;
    }

    // --- CSI sequences: ESC [ ... ---
    if (seq1 == '[') {
        return read_csi_sequence();
    }

    // --- ESC + letter = Alt+letter (legacy, non-Kitty terminals) ---
    if (seq1 == 'b') return CommandEvent{Command::MoveWordLeft};
    if (seq1 == 'f') return CommandEvent{Command::MoveWordRight};

    return std::nullopt;
}

/// Read a complete UTF-8 character starting with the given first byte.
/// Uses the byte length encoded in the leading byte per UTF-8 spec.
std::string read_utf8_char(int first_byte) {
    std::string result;
    result += static_cast<char>(first_byte);

    // Determine expected byte count from the leading byte.
    int remaining = 0;
    if ((first_byte & 0xE0) == 0xC0) remaining = 1;      // 2-byte: 110xxxxx
    else if ((first_byte & 0xF0) == 0xE0) remaining = 2;  // 3-byte: 1110xxxx
    else if ((first_byte & 0xF8) == 0xF0) remaining = 3;  // 4-byte: 11110xxx

    for (int i = 0; i < remaining; ++i) {
        int b = read_byte();
        if (b == -1) break;
        result += static_cast<char>(b);
    }
    return result;
}

} // anonymous namespace

std::optional<CommandEvent> read_key_event(InputMode mode) {
    int c = read_byte();
    if (c == -1) return std::nullopt;

    // Escape sequences.
    if (c == '\x1b') {
        return read_escape_sequence();
    }

    // UTF-8 multi-byte character (first byte >= 0x80).
    if (c >= 0x80) {
        std::string utf8 = read_utf8_char(c);
        CommandEvent evt;
        evt.command = Command::InsertText;
        evt.text = utf8;
        return evt;
    }

    // QuitConfirm mode: pass y/n as InsertChar, ESC handled above.
    if (mode == InputMode::QuitConfirm) {
        if (c >= 32 && c < 127) return CommandEvent{Command::InsertChar, static_cast<char>(c)};
        return std::nullopt;
    }

    // PinnedList mode: arrow keys handled via ESC sequences above,
    // here handle Enter, Delete, 'a', ESC (as raw bytes).
    if (mode == InputMode::PinnedList) {
        if (c == '\r' || c == '\n') return CommandEvent{Command::Copy};
        if (c == 127 || c == 8) return CommandEvent{Command::DeleteForward};
        if (c == 'a' || c == 'A') return CommandEvent{Command::SelectAll};
        if (c == 'x' || c == 'X') return CommandEvent{Command::ClearPinned};
        // Ctrl keys still work (Ctrl-L to close, Ctrl-B, etc.)
        if (c >= 1 && c <= 26) {
            switch (c) {
                case 2:  return CommandEvent{Command::PinSelection};
                case 12: return CommandEvent{Command::ShowPinnedList};
                case 17: return CommandEvent{Command::Quit};
            }
        }
        return std::nullopt;
    }

    // Non-normal modes: pass through chars for prompt handling.
    if (mode != InputMode::Normal) {
        if (c == '\r' || c == '\n') return CommandEvent{Command::InsertNewline};
        if (c == 127 || c == 8) return CommandEvent{Command::DeleteBackward};
        if (c >= 32 && c < 127) return CommandEvent{Command::InsertChar, static_cast<char>(c)};
        return std::nullopt;
    }

    // Enter (\r=13) and Tab (\t=9) fall in 1-26 range — check before Ctrl keys.
    if (c == '\r' || c == '\n') return CommandEvent{Command::InsertNewline};
    if (c == '\t') return CommandEvent{Command::InsertChar, '\t'};

    // Ctrl key combinations (Ctrl-A = 1, Ctrl-Z = 26).
    // Legacy path: used when Kitty keyboard protocol is not active.
    if (c >= 1 && c <= 26) {
        switch (c) {
            case 1:  return CommandEvent{Command::SelectAll};              // Ctrl-A
            case 2:  return CommandEvent{Command::PinSelection};           // Ctrl-B
            case 3:  return CommandEvent{Command::Copy};                   // Ctrl-C
            case 4:  return CommandEvent{Command::DeleteLine};             // Ctrl-D
            case 6:  return CommandEvent{Command::OpenSearch};             // Ctrl-F
            case 8:  return CommandEvent{Command::OpenReplace};            // Ctrl-H
            case 12: return CommandEvent{Command::ShowPinnedList};         // Ctrl-L
            case 23: return CommandEvent{Command::SelectNextOccurrence};   // Ctrl-W
            case 16: return CommandEvent{Command::OpenCommandPalette};     // Ctrl-P
            case 17: return CommandEvent{Command::Quit};                   // Ctrl-Q
            case 19: return CommandEvent{Command::Save};                   // Ctrl-S
            case 22: return CommandEvent{Command::Paste};                  // Ctrl-V
            case 24: return CommandEvent{Command::Cut};                    // Ctrl-X
            case 25: return CommandEvent{Command::Redo};                   // Ctrl-Y
            case 26: return CommandEvent{Command::Undo};                   // Ctrl-Z
        }
        return std::nullopt;
    }

    // Backspace.
    if (c == 127 || c == 8) return CommandEvent{Command::DeleteBackward};

    // Printable ASCII character.
    if (c >= 32 && c < 127) return CommandEvent{Command::InsertChar, static_cast<char>(c)};

    return std::nullopt;
}

} // namespace beresta
