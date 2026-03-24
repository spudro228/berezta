#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace berezta {

/// Abstract codec interface for encoding/decoding between file bytes and
/// internal UTF-8 representation.
class Codec {
public:
    virtual ~Codec() = default;

    /// Human-readable name of this encoding (e.g. "UTF-8", "Latin-1").
    virtual std::string name() const = 0;

    /// Decode raw file bytes into internal UTF-8.
    virtual std::string decode(const std::string& bytes) const = 0;

    /// Encode internal UTF-8 into file bytes.
    virtual std::string encode(const std::string& utf8) const = 0;

    /// Detect BOM at start of `bytes`; return number of bytes to skip.
    virtual size_t detect_bom(const std::string& bytes) const;
};

// ---- Concrete codecs -------------------------------------------------------

/// UTF-8 pass-through codec. Strips BOM on decode, optionally adds on encode.
class Utf8Codec : public Codec {
public:
    std::string name() const override;
    std::string decode(const std::string& bytes) const override;
    std::string encode(const std::string& utf8) const override;
    size_t detect_bom(const std::string& bytes) const override;
};

/// ISO 8859-1 (Latin-1) codec.
/// Each byte 0x00-0xFF maps directly to U+0000-U+00FF.
class Latin1Codec : public Codec {
public:
    std::string name() const override;
    std::string decode(const std::string& bytes) const override;
    std::string encode(const std::string& utf8) const override;
};

/// Windows-1251 (Cyrillic) codec.
class Cp1251Codec : public Codec {
public:
    std::string name() const override;
    std::string decode(const std::string& bytes) const override;
    std::string encode(const std::string& utf8) const override;
};

// ---- Factory / registry ----------------------------------------------------

/// Create a codec by name ("utf-8", "latin-1", "cp1251").
/// Returns nullptr if the name is unknown.
std::unique_ptr<Codec> create_codec(const std::string& name);

/// Try to detect the encoding of `bytes` (BOM then heuristic).
/// Falls back to UTF-8 if no clear signal.
std::unique_ptr<Codec> detect_codec(const std::string& bytes);

/// Return the list of available codec names.
std::vector<std::string> available_codecs();

} // namespace berezta
