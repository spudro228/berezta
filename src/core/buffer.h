#pragma once

#include "core/codec.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace berezta {

/// A text buffer storing lines as a vector of strings.
///
/// Each line is stored WITHOUT a trailing newline character.
/// An empty buffer always has at least one (empty) line.
/// Internally, all text is UTF-8. The codec is used only for file I/O.
class Buffer {
public:
    Buffer();
    explicit Buffer(const std::string& text);

    /// Load from a file on disk with optional encoding specification.
    /// If encoding is "auto" (default), the codec is detected automatically.
    static Buffer from_file(const std::string& path, const std::string& encoding = "auto");

    /// Save to a file on disk using the stored codec. Marks as unmodified.
    void save_to_file(const std::string& path);

    /// Save to a file on disk using a specific codec.
    void save_to_file(const std::string& path, const Codec& codec);

    // --- Queries ---

    size_t line_count() const { return lines_.size(); }
    size_t total_chars() const;
    bool is_empty() const;
    bool is_modified() const { return modified_; }
    void mark_saved() { modified_ = false; }

    /// Get a reference to the line at the given index (0-based).
    const std::string& line(size_t line_idx) const;

    /// Width of a line in bytes (excluding newline).
    size_t line_width(size_t line_idx) const;

    /// Display width of a line in terminal columns (accounts for multi-byte
    /// and double-width characters).
    size_t line_display_width(size_t line_idx) const;

    /// Convert a (line, col) pair to a byte offset.
    /// Column is clamped to line byte width.
    size_t pos_to_char(size_t line, size_t col) const;

    /// Convert a byte offset to (line, col_bytes).
    std::pair<size_t, size_t> char_to_pos(size_t char_idx) const;

    /// Extract a substring between two byte offsets.
    std::string slice(size_t from, size_t to) const;

    /// Convert a display column to a byte offset within a line.
    size_t display_col_to_byte(size_t line_idx, size_t display_col) const;

    /// Convert a byte offset within a line to a display column.
    size_t byte_to_display_col(size_t line_idx, size_t byte_offset_in_line) const;

    // --- Mutations ---

    /// Insert text at the given byte offset.
    void insert(size_t char_idx, const std::string& text);

    /// Remove bytes in the range [from, to).
    void remove(size_t from, size_t to);

    /// Replace bytes in the range [from, to) with new text.
    void replace(size_t from, size_t to, const std::string& text);

    /// Get all text as a single string (with newlines between lines).
    std::string to_string() const;

    // --- Codec ---

    /// Get the codec used for file I/O.
    const Codec& codec() const;

    /// Set a new codec for file I/O.
    void set_codec(std::unique_ptr<Codec> codec);

private:
    std::vector<std::string> lines_;
    bool modified_ = false;
    std::unique_ptr<Codec> codec_;

    /// Re-parse a flat string into lines.
    void set_text(const std::string& text);
};

} // namespace berezta
