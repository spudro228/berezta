#include "core/history.h"

namespace beresta {

void History::record(Edit edit) {
    undo_stack_.push_back(std::move(edit));
    redo_stack_.clear();
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
