#pragma once

#include "core/selection.h"

#include <string>
#include <vector>

namespace beresta {

/// A single atomic edit operation that can be undone/redone.
///
/// Stores full buffer snapshots for simplicity and correctness,
/// especially with multi-cursor edits. This is O(file_size) per edit,
/// which is perfectly fine for the small files beresta targets.
struct Edit {
    std::string buffer_before;
    std::string buffer_after;
    Selection selection_before;
    Selection selection_after;
};

/// Manages undo/redo stacks of edit operations.
///
/// When a new edit is recorded, the redo stack is cleared
/// (standard undo behavior — branching is not supported).
class History {
public:
    History() = default;

    /// Record an edit that was just applied.
    void record(Edit edit);

    /// Pop the most recent edit for undoing.
    bool undo(Edit& out);

    /// Pop the most recent undone edit for redoing.
    bool redo(Edit& out);

    bool can_undo() const { return !undo_stack_.empty(); }
    bool can_redo() const { return !redo_stack_.empty(); }

    void clear();

private:
    std::vector<Edit> undo_stack_;
    std::vector<Edit> redo_stack_;
};

} // namespace beresta
