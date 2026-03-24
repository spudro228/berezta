#include "core/codec.h"

#include <algorithm>
#include <array>
#include <unordered_map>
#include <utf8.h>

namespace berezta {

// ---- Codec base ------------------------------------------------------------

size_t Codec::detect_bom(const std::string& /*bytes*/) const {
    return 0;
}

// ---- Utf8Codec -------------------------------------------------------------

std::string Utf8Codec::name() const { return "UTF-8"; }

size_t Utf8Codec::detect_bom(const std::string& bytes) const {
    // UTF-8 BOM: EF BB BF
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return 3;
    }
    return 0;
}

std::string Utf8Codec::decode(const std::string& bytes) const {
    size_t bom = detect_bom(bytes);
    if (bom > 0) {
        return bytes.substr(bom);
    }
    return bytes;
}

std::string Utf8Codec::encode(const std::string& utf8) const {
    return utf8; // no BOM added on save
}

// ---- Latin1Codec -----------------------------------------------------------

std::string Latin1Codec::name() const { return "Latin-1"; }

std::string Latin1Codec::decode(const std::string& bytes) const {
    std::string result;
    result.reserve(bytes.size() * 2); // worst case
    for (unsigned char b : bytes) {
        if (b < 0x80) {
            result.push_back(static_cast<char>(b));
        } else {
            // U+0080..U+00FF -> 2-byte UTF-8: 110000xx 10xxxxxx
            uint32_t cp = b;
            utf8::append(cp, std::back_inserter(result));
        }
    }
    return result;
}

std::string Latin1Codec::encode(const std::string& utf8_text) const {
    std::string result;
    result.reserve(utf8_text.size());
    auto it = utf8_text.begin();
    auto end = utf8_text.end();
    while (it != end) {
        uint32_t cp = utf8::next(it, end);
        if (cp <= 0xFF) {
            result.push_back(static_cast<char>(static_cast<unsigned char>(cp)));
        } else {
            result.push_back('?'); // cannot represent
        }
    }
    return result;
}

// ---- Cp1251Codec -----------------------------------------------------------

namespace {

// CP1251 mapping for bytes 0x80-0xBF -> Unicode codepoints.
// Bytes 0xC0-0xFF map linearly: 0xC0->U+0410 .. 0xFF->U+044F.
static constexpr std::array<uint32_t, 64> kCp1251_80BF = {{
    0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, // 80-87
    0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F, // 88-8F
    0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, // 90-97
    0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, // 98-9F  (0x98=undefined)
    0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, // A0-A7
    0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x042F, // A8-AF
    0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7, // B0-B7
    0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x045F, // B8-BF
}};

/// Build a reverse map: Unicode codepoint -> CP1251 byte.
std::unordered_map<uint32_t, unsigned char> build_cp1251_reverse() {
    std::unordered_map<uint32_t, unsigned char> m;
    // 0x80-0xBF
    for (size_t i = 0; i < 64; ++i) {
        uint32_t cp = kCp1251_80BF[i];
        if (cp != 0xFFFD) {
            unsigned char byte = static_cast<unsigned char>(0x80 + i);
            // If there's a duplicate mapping, keep the first one
            m.emplace(cp, byte);
        }
    }
    // 0xC0-0xFF -> U+0410-U+044F
    for (unsigned int i = 0; i < 64; ++i) {
        m[0x0410 + i] = static_cast<unsigned char>(0xC0 + i);
    }
    return m;
}

const std::unordered_map<uint32_t, unsigned char>& cp1251_reverse() {
    static const auto map = build_cp1251_reverse();
    return map;
}

} // anonymous namespace

std::string Cp1251Codec::name() const { return "CP1251"; }

std::string Cp1251Codec::decode(const std::string& bytes) const {
    std::string result;
    result.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        if (b < 0x80) {
            result.push_back(static_cast<char>(b));
        } else {
            uint32_t cp;
            if (b >= 0xC0) {
                // 0xC0-0xFF -> U+0410-U+044F (А-я)
                cp = 0x0410 + (b - 0xC0);
            } else {
                // 0x80-0xBF: lookup table
                cp = kCp1251_80BF[b - 0x80];
            }
            utf8::append(cp, std::back_inserter(result));
        }
    }
    return result;
}

std::string Cp1251Codec::encode(const std::string& utf8_text) const {
    const auto& rev = cp1251_reverse();
    std::string result;
    result.reserve(utf8_text.size());
    auto it = utf8_text.begin();
    auto end = utf8_text.end();
    while (it != end) {
        uint32_t cp = utf8::next(it, end);
        if (cp < 0x80) {
            result.push_back(static_cast<char>(cp));
        } else {
            auto found = rev.find(cp);
            if (found != rev.end()) {
                result.push_back(static_cast<char>(found->second));
            } else {
                result.push_back('?');
            }
        }
    }
    return result;
}

// ---- Factory / registry ----------------------------------------------------

std::unique_ptr<Codec> create_codec(const std::string& codec_name) {
    // Normalize to lowercase for matching.
    std::string lower = codec_name;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (lower == "utf-8" || lower == "utf8") {
        return std::make_unique<Utf8Codec>();
    }
    if (lower == "latin-1" || lower == "latin1" || lower == "iso-8859-1" || lower == "iso88591") {
        return std::make_unique<Latin1Codec>();
    }
    if (lower == "cp1251" || lower == "windows-1251" || lower == "win1251") {
        return std::make_unique<Cp1251Codec>();
    }
    return nullptr;
}

std::unique_ptr<Codec> detect_codec(const std::string& bytes) {
    // 1. Check for UTF-8 BOM.
    if (bytes.size() >= 3 &&
        static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        return std::make_unique<Utf8Codec>();
    }

    // 2. Check if it's valid UTF-8.
    if (utf8::is_valid(bytes.begin(), bytes.end())) {
        return std::make_unique<Utf8Codec>();
    }

    // 3. Heuristic: look for CP1251-specific byte patterns.
    //    CP1251 Cyrillic letters live in 0xC0-0xFF. If we see many of those
    //    with typical Russian letter frequencies, pick CP1251.
    size_t cp1251_cyrillic = 0;
    size_t high_bytes = 0;
    for (unsigned char b : bytes) {
        if (b >= 0x80) {
            high_bytes++;
            if (b >= 0xC0) {
                cp1251_cyrillic++;
            }
        }
    }

    if (high_bytes > 0 && cp1251_cyrillic * 100 / high_bytes > 70) {
        return std::make_unique<Cp1251Codec>();
    }

    // 4. Fallback: Latin-1 (any byte sequence is valid Latin-1).
    if (high_bytes > 0) {
        return std::make_unique<Latin1Codec>();
    }

    // 5. Default: UTF-8.
    return std::make_unique<Utf8Codec>();
}

std::vector<std::string> available_codecs() {
    return {"UTF-8", "Latin-1", "CP1251"};
}

} // namespace berezta
