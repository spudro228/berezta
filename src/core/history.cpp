#include "core/history.h"

namespace beresta {

void History::record(Edit edit, bool mergeable) {
    if (mergeable && last_mergeable_ && !undo_stack_.empty()) {
        // Coalesce: keep original before-state, update after-state.
        undo_stack_.back().buffer_after = std::move(edit.buffer_after);
        undo_stack_.back().selection_after = edit.selection_after;
    } else {
        undo_stack_.push_back(std::move(edit));
    }
    redo_stack_.clear();
    last_mergeable_ = mergeable;
}

bool History::undo(Edit& out) {
    if (undo_stack_.empty()) {
        return false;
    }
    out = std::move(undo_stack_.back());
    undo_stack_.pop_back();
    redo_stack_.push_back(out);
    return true;
}

bool History::redo(Edit& out) {
    if (redo_stack_.empty()) {
        return false;
    }
    out = std::move(redo_stack_.back());
    redo_stack_.pop_back();
    undo_stack_.push_back(out);
    return true;
}

void History::clear() {
    undo_stack_.clear();
    redo_stack_.clear();
}

} // namespace beresta
