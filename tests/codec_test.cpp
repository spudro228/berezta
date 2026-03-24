#include "core/codec.h"
#include "core/buffer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace berezta;

// ---- Utf8Codec tests -------------------------------------------------------

TEST(CodecTest, Utf8Name) {
    Utf8Codec codec;
    EXPECT_EQ(codec.name(), "UTF-8");
}

TEST(CodecTest, Utf8DecodePassthrough) {
    Utf8Codec codec;
    std::string input = "Hello, world!";
    EXPECT_EQ(codec.decode(input), input);
}

TEST(CodecTest, Utf8DecodeStripsBOM) {
    Utf8Codec codec;
    std::string bom = "\xEF\xBB\xBF";
    std::string input = bom + "Hello";
    EXPECT_EQ(codec.decode(input), "Hello");
}

TEST(CodecTest, Utf8EncodePassthrough) {
    Utf8Codec codec;
    std::string input = "Hello, world!";
    EXPECT_EQ(codec.encode(input), input);
}

TEST(CodecTest, Utf8RoundtripMultibyte) {
    Utf8Codec codec;
    // Russian text in UTF-8
    std::string input = u8"Привет мир";
    EXPECT_EQ(codec.decode(codec.encode(input)), input);
}

TEST(CodecTest, Utf8DetectBOM) {
    Utf8Codec codec;
    std::string with_bom = "\xEF\xBB\xBFhello";
    EXPECT_EQ(codec.detect_bom(with_bom), 3u);

    std::string without_bom = "hello";
    EXPECT_EQ(codec.detect_bom(without_bom), 0u);
}

// ---- Latin1Codec tests -----------------------------------------------------

TEST(CodecTest, Latin1Name) {
    Latin1Codec codec;
    EXPECT_EQ(codec.name(), "Latin-1");
}

TEST(CodecTest, Latin1DecodeAscii) {
    Latin1Codec codec;
    std::string input = "Hello";
    EXPECT_EQ(codec.decode(input), "Hello");
}

TEST(CodecTest, Latin1DecodeHighBytes) {
    Latin1Codec codec;
    // 0xE9 = U+00E9 = e with acute
    std::string input(1, '\xE9');
    std::string expected = u8"\u00E9"; // UTF-8 encoding of U+00E9
    EXPECT_EQ(codec.decode(input), expected);
}

TEST(CodecTest, Latin1DecodeRange) {
    Latin1Codec codec;
    // Test several bytes in 0x80-0xFF range
    std::string input;
    input += '\xA0'; // NBSP = U+00A0
    input += '\xBF'; // inverted question mark = U+00BF
    input += '\xFF'; // y with diaeresis = U+00FF

    std::string decoded = codec.decode(input);
    std::string expected = u8"\u00A0\u00BF\u00FF";
    EXPECT_EQ(decoded, expected);
}

TEST(CodecTest, Latin1EncodeAscii) {
    Latin1Codec codec;
    EXPECT_EQ(codec.encode("Hello"), "Hello");
}

TEST(CodecTest, Latin1EncodeHighCodepoints) {
    Latin1Codec codec;
    std::string utf8 = u8"\u00E9"; // e-acute
    std::string encoded = codec.encode(utf8);
    EXPECT_EQ(encoded.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(encoded[0]), 0xE9u);
}

TEST(CodecTest, Latin1EncodeUnencodableBecomesQuestion) {
    Latin1Codec codec;
    // U+0410 = Cyrillic A, not in Latin-1
    std::string utf8 = u8"\u0410";
    std::string encoded = codec.encode(utf8);
    EXPECT_EQ(encoded, "?");
}

TEST(CodecTest, Latin1Roundtrip) {
    Latin1Codec codec;
    // All bytes 0x00-0xFF should roundtrip
    std::string raw;
    for (int i = 0; i < 256; ++i) {
        raw.push_back(static_cast<char>(i));
    }
    std::string utf8 = codec.decode(raw);
    std::string back = codec.encode(utf8);
    EXPECT_EQ(back, raw);
}

// ---- Cp1251Codec tests -----------------------------------------------------

TEST(CodecTest, Cp1251Name) {
    Cp1251Codec codec;
    EXPECT_EQ(codec.name(), "CP1251");
}

TEST(CodecTest, Cp1251DecodeAscii) {
    Cp1251Codec codec;
    EXPECT_EQ(codec.decode("Hello"), "Hello");
}

TEST(CodecTest, Cp1251DecodeCyrillicUppercase) {
    Cp1251Codec codec;
    // 0xC0 = A (U+0410), 0xC1 = Б (U+0411)
    std::string input;
    input += '\xC0';
    input += '\xC1';
    std::string decoded = codec.decode(input);
    std::string expected = u8"\u0410\u0411"; // АБ
    EXPECT_EQ(decoded, expected);
}

TEST(CodecTest, Cp1251DecodeCyrillicLowercase) {
    Cp1251Codec codec;
    // 0xE0 = а (U+0430), 0xE1 = б (U+0431)
    std::string input;
    input += '\xE0';
    input += '\xE1';
    std::string decoded = codec.decode(input);
    std::string expected = u8"\u0430\u0431";
    EXPECT_EQ(decoded, expected);
}

TEST(CodecTest, Cp1251DecodeSpecialChars) {
    Cp1251Codec codec;
    // 0xA8 = Ё (U+0401), 0xB8 = ё (U+0451)
    std::string input;
    input += '\xA8';
    input += '\xB8';
    std::string decoded = codec.decode(input);
    std::string expected = u8"\u0401\u0451"; // Ёё
    EXPECT_EQ(decoded, expected);
}

TEST(CodecTest, Cp1251EncodeCyrillic) {
    Cp1251Codec codec;
    std::string utf8 = u8"\u0410\u0411\u0430\u0431"; // АБаб
    std::string encoded = codec.encode(utf8);
    EXPECT_EQ(encoded.size(), 4u);
    EXPECT_EQ(static_cast<unsigned char>(encoded[0]), 0xC0u);
    EXPECT_EQ(static_cast<unsigned char>(encoded[1]), 0xC1u);
    EXPECT_EQ(static_cast<unsigned char>(encoded[2]), 0xE0u);
    EXPECT_EQ(static_cast<unsigned char>(encoded[3]), 0xE1u);
}

TEST(CodecTest, Cp1251RoundtripCyrillicRange) {
    Cp1251Codec codec;
    // Bytes 0xC0-0xFF (Cyrillic letters) should roundtrip
    std::string raw;
    for (int i = 0xC0; i <= 0xFF; ++i) {
        raw.push_back(static_cast<char>(i));
    }
    std::string utf8 = codec.decode(raw);
    std::string back = codec.encode(utf8);
    EXPECT_EQ(back, raw);
}

TEST(CodecTest, Cp1251EncodeUnencodableBecomesQuestion) {
    Cp1251Codec codec;
    // U+4E2D = Chinese character, not in CP1251
    std::string utf8 = u8"\u4E2D";
    std::string encoded = codec.encode(utf8);
    EXPECT_EQ(encoded, "?");
}

// ---- Codec detection tests -------------------------------------------------

TEST(CodecTest, DetectUtf8BOM) {
    std::string bytes = "\xEF\xBB\xBFHello";
    auto codec = detect_codec(bytes);
    EXPECT_EQ(codec->name(), "UTF-8");
}

TEST(CodecTest, DetectValidUtf8) {
    std::string bytes = u8"Привет мир"; // valid UTF-8
    auto codec = detect_codec(bytes);
    EXPECT_EQ(codec->name(), "UTF-8");
}

TEST(CodecTest, DetectAscii) {
    std::string bytes = "Hello world";
    auto codec = detect_codec(bytes);
    EXPECT_EQ(codec->name(), "UTF-8"); // ASCII is valid UTF-8
}

TEST(CodecTest, DetectCp1251Heuristic) {
    // Build a string of CP1251 Cyrillic bytes (0xC0-0xFF) which is NOT valid UTF-8
    std::string bytes;
    // "Привет" in CP1251: П=0xCF р=0xF0 и=0xE8 в=0xE2 е=0xE5 т=0xF2
    bytes += '\xCF';
    bytes += '\xF0';
    bytes += '\xE8';
    bytes += '\xE2';
    bytes += '\xE5';
    bytes += '\xF2';
    auto codec = detect_codec(bytes);
    EXPECT_EQ(codec->name(), "CP1251");
}

TEST(CodecTest, DetectLatin1Fallback) {
    // Build bytes with high bytes that are NOT valid UTF-8 and NOT mostly
    // in the CP1251 Cyrillic range (0xC0-0xFF).
    std::string bytes;
    bytes += '\x80'; // not in Cyrillic range
    bytes += '\x81';
    bytes += '\x82';
    bytes += '\x83';
    bytes += '\x84';
    auto codec = detect_codec(bytes);
    EXPECT_EQ(codec->name(), "Latin-1");
}

// ---- Factory tests ---------------------------------------------------------

TEST(CodecTest, CreateCodecUtf8) {
    auto codec = create_codec("utf-8");
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->name(), "UTF-8");
}

TEST(CodecTest, CreateCodecUtf8CaseInsensitive) {
    auto codec = create_codec("UTF-8");
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->name(), "UTF-8");
}

TEST(CodecTest, CreateCodecLatin1) {
    auto codec = create_codec("latin-1");
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->name(), "Latin-1");
}

TEST(CodecTest, CreateCodecCp1251) {
    auto codec = create_codec("cp1251");
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->name(), "CP1251");
}

TEST(CodecTest, CreateCodecWindows1251) {
    auto codec = create_codec("windows-1251");
    ASSERT_NE(codec, nullptr);
    EXPECT_EQ(codec->name(), "CP1251");
}

TEST(CodecTest, CreateCodecUnknown) {
    auto codec = create_codec("unknown-encoding");
    EXPECT_EQ(codec, nullptr);
}

TEST(CodecTest, AvailableCodecs) {
    auto codecs = available_codecs();
    EXPECT_EQ(codecs.size(), 3u);
    EXPECT_EQ(codecs[0], "UTF-8");
    EXPECT_EQ(codecs[1], "Latin-1");
    EXPECT_EQ(codecs[2], "CP1251");
}

// ---- Buffer + Codec integration tests --------------------------------------

TEST(CodecTest, BufferDefaultCodecIsUtf8) {
    Buffer buf;
    EXPECT_EQ(buf.codec().name(), "UTF-8");
}

TEST(CodecTest, BufferFromStringCodecIsUtf8) {
    Buffer buf("hello");
    EXPECT_EQ(buf.codec().name(), "UTF-8");
}

TEST(CodecTest, BufferSetCodec) {
    Buffer buf;
    buf.set_codec(std::make_unique<Latin1Codec>());
    EXPECT_EQ(buf.codec().name(), "Latin-1");
}

TEST(CodecTest, BufferSaveLoadUtf8Roundtrip) {
    auto path = std::filesystem::temp_directory_path() / "berezta_codec_test_utf8.txt";
    std::string content = u8"Привет мир";

    {
        Buffer buf(content);
        buf.save_to_file(path.string());
    }

    {
        Buffer loaded = Buffer::from_file(path.string());
        EXPECT_EQ(loaded.to_string(), content);
        EXPECT_EQ(loaded.codec().name(), "UTF-8");
    }

    std::filesystem::remove(path);
}

TEST(CodecTest, BufferSaveLoadLatin1Roundtrip) {
    auto path = std::filesystem::temp_directory_path() / "berezta_codec_test_latin1.txt";

    // Write Latin-1 encoded bytes directly.
    {
        std::ofstream f(path, std::ios::binary);
        // "caf\xE9" in Latin-1 = "cafe" with e-acute
        f << "caf\xE9";
    }

    // Load with explicit Latin-1 encoding.
    {
        Buffer loaded = Buffer::from_file(path.string(), "latin-1");
        std::string expected = u8"caf\u00E9";
        EXPECT_EQ(loaded.to_string(), expected);
        EXPECT_EQ(loaded.codec().name(), "Latin-1");
    }

    std::filesystem::remove(path);
}

TEST(CodecTest, BufferSaveWithCodec) {
    auto path = std::filesystem::temp_directory_path() / "berezta_codec_test_save.txt";
    std::string content = u8"caf\u00E9"; // "cafe" with e-acute

    {
        Buffer buf(content);
        Latin1Codec latin1;
        buf.save_to_file(path.string(), latin1);
    }

    // Read raw bytes back.
    {
        std::ifstream f(path, std::ios::binary);
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string raw = ss.str();
        EXPECT_EQ(raw.size(), 4u); // "caf" + one byte for e-acute
        EXPECT_EQ(raw, "caf\xE9");
    }

    std::filesystem::remove(path);
}

TEST(CodecTest, BufferSaveLoadCp1251Roundtrip) {
    auto path = std::filesystem::temp_directory_path() / "berezta_codec_test_cp1251.txt";

    // Write CP1251 encoded bytes directly: "Привет" in CP1251
    {
        std::ofstream f(path, std::ios::binary);
        f << "\xCF\xF0\xE8\xE2\xE5\xF2"; // Привет
    }

    // Load with explicit CP1251 encoding.
    {
        Buffer loaded = Buffer::from_file(path.string(), "cp1251");
        std::string expected = u8"Привет";
        EXPECT_EQ(loaded.to_string(), expected);
        EXPECT_EQ(loaded.codec().name(), "CP1251");
    }

    std::filesystem::remove(path);
}

// ---- Buffer display width helpers ------------------------------------------

TEST(CodecTest, BufferLineDisplayWidthAscii) {
    Buffer buf("hello");
    EXPECT_EQ(buf.line_display_width(0), 5u);
}

TEST(CodecTest, BufferLineDisplayWidthMultibyte) {
    Buffer buf(u8"Привет"); // 6 Cyrillic chars, each 1 display column
    EXPECT_EQ(buf.line_display_width(0), 6u);
    EXPECT_EQ(buf.line_width(0), 12u); // 12 bytes (2 bytes per char)
}

TEST(CodecTest, BufferByteToDisplayCol) {
    Buffer buf(u8"Привет"); // Each char is 2 bytes
    EXPECT_EQ(buf.byte_to_display_col(0, 0), 0u);
    EXPECT_EQ(buf.byte_to_display_col(0, 2), 1u); // After first char
    EXPECT_EQ(buf.byte_to_display_col(0, 4), 2u);
    EXPECT_EQ(buf.byte_to_display_col(0, 12), 6u); // End of line
}

TEST(CodecTest, BufferDisplayColToByte) {
    Buffer buf(u8"Привет");
    EXPECT_EQ(buf.display_col_to_byte(0, 0), 0u);
    EXPECT_EQ(buf.display_col_to_byte(0, 1), 2u);
    EXPECT_EQ(buf.display_col_to_byte(0, 2), 4u);
    EXPECT_EQ(buf.display_col_to_byte(0, 6), 12u);
}

TEST(CodecTest, BufferFromFileAutoDetect) {
    auto path = std::filesystem::temp_directory_path() / "berezta_codec_auto.txt";

    // Write valid UTF-8
    {
        std::ofstream f(path, std::ios::binary);
        f << u8"Hello мир";
    }

    {
        Buffer loaded = Buffer::from_file(path.string()); // auto detect
        EXPECT_EQ(loaded.to_string(), u8"Hello мир");
        EXPECT_EQ(loaded.codec().name(), "UTF-8");
    }

    std::filesystem::remove(path);
}
