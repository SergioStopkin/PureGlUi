// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureGlUi. PureGlUi is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureGlUi is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureGlUi. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestUnicode.cpp
 * @brief Unit tests for Common::Unicode - the UTF-8/16/32 <-> wide conversions
 *        behind the text pipeline. Covers 1/2/3/4-byte UTF-8, surrogate handling
 *        (UTF-16), round trips across all encodings, and malformed-input
 *        replacement (U+FFFD).
 */

#include "common/unicode.h"

#include <cstdint>
#include <gtest/gtest.h>
#include <string>

using Common::Unicode;

namespace {
constexpr std::uint32_t REPLACEMENT = 0xFFFDU;
const std::string       kEuro       = "\xE2\x82\xAC";     // U+20AC, 3-byte
const std::string       kEmoji      = "\xF0\x9F\x98\x80"; // U+1F600, 4-byte
} // namespace

TEST(Unicode, AsciiRoundTrip)
{
    const std::string ascii = "Hello, World!";
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf8(ascii)), ascii);
    EXPECT_EQ(Unicode::fromUtf8("A").size(), 1U);
    EXPECT_EQ(static_cast<std::uint32_t>(Unicode::fromUtf8("A")[0]), 65U);
}

TEST(Unicode, TwoAndThreeByteRoundTrip)
{
    const std::string accented = "caf\xC3\xA9"; // "cafe" with U+00E9
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf8(accented)), accented);
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf8(kEuro)), kEuro);
}

TEST(Unicode, FourByteToUtf32)
{
    const std::u32string cp = Unicode::toUtf32(Unicode::fromUtf8(kEmoji));
    ASSERT_EQ(cp.size(), 1U);
    EXPECT_EQ(static_cast<std::uint32_t>(cp[0]), 0x1F600U);
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf8(kEmoji)), kEmoji);
}

TEST(Unicode, Utf16SurrogatePairRoundTrip)
{
    const std::u16string u16 = Unicode::toUtf16(Unicode::fromUtf8(kEmoji));
    EXPECT_EQ(u16.size(), 2U); // astral codepoint -> surrogate pair
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf16(u16)), kEmoji);
}

TEST(Unicode, Utf32SourceRoundTrip)
{
    std::u32string src;
    src.push_back(static_cast<char32_t>(0x1F600));
    EXPECT_EQ(Unicode::toUtf8(Unicode::fromUtf32(src)), kEmoji);
}

TEST(Unicode, InvalidLeadByteBecomesReplacement)
{
    const std::wstring w = Unicode::fromUtf8("\xFF");
    ASSERT_EQ(w.size(), 1U);
    EXPECT_EQ(static_cast<std::uint32_t>(w[0]), REPLACEMENT);
}

TEST(Unicode, TruncatedSequenceBecomesReplacement)
{
    // Lead byte of a 3-byte sequence with no continuation bytes.
    const std::wstring w = Unicode::fromUtf8("\xE2");
    ASSERT_EQ(w.size(), 1U);
    EXPECT_EQ(static_cast<std::uint32_t>(w[0]), REPLACEMENT);
}

TEST(Unicode, EmptyInputEmptyOutput)
{
    EXPECT_TRUE(Unicode::fromUtf8("").empty());
    EXPECT_TRUE(Unicode::toUtf8(std::wstring {}).empty());
}
