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

#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace Common {

// Single-source unicode conversions. Canonical text type is std::wstring.
// On platforms where sizeof(wchar_t) == 2 (Windows), wstring carries UTF-16
// (surrogate pairs for codepoints >= U+10000). On 4-byte platforms (Linux,
// macOS), wstring carries UTF-32 codepoints. All conversions handle the
// surrogate encoding/decoding transparently.
class Unicode final {
public:
    Unicode()  = delete;
    ~Unicode() = delete;

    Unicode(const Unicode &)             = delete;
    Unicode(Unicode &&)                  = delete;
    Unicode & operator=(const Unicode &) = delete;
    Unicode & operator=(Unicode &&)      = delete;

    static std::wstring fromUtf8(std::string_view text)
    {
        std::wstring out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            appendCodepointToWide(out, decodeUtf8(text, i));
        }
        return out;
    }

    static std::string toUtf8(std::wstring_view text)
    {
        std::string out;
        out.reserve(text.size() * 2);
        size_t i = 0;
        while (i < text.size()) {
            appendCodepointToUtf8(out, decodeWide(text, i));
        }
        return out;
    }

    static std::wstring fromUtf16(std::u16string_view text)
    {
        std::wstring out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            appendCodepointToWide(out, decodeUtf16(text, i));
        }
        return out;
    }

    static std::u16string toUtf16(std::wstring_view text)
    {
        std::u16string out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            appendCodepointToUtf16(out, decodeWide(text, i));
        }
        return out;
    }

    static std::wstring fromUtf32(std::u32string_view text)
    {
        std::wstring out;
        out.reserve(text.size());
        for (const char32_t cp : text) {
            appendCodepointToWide(out, static_cast<uint32_t>(cp));
        }
        return out;
    }

    static std::u32string toUtf32(std::wstring_view text)
    {
        std::u32string out;
        out.reserve(text.size());
        size_t i = 0;
        while (i < text.size()) {
            out.push_back(static_cast<char32_t>(decodeWide(text, i)));
        }
        return out;
    }

private:
    static constexpr uint32_t REPLACEMENT = 0xFFFDU;

    static uint32_t decodeUtf8(std::string_view text, size_t & i)
    {
        const auto b = static_cast<unsigned char>(text[i]);
        uint32_t   cp;
        size_t     extra;
        if (b < 0x80U) {
            cp    = b;
            extra = 0;
        } else if ((b & 0xE0U) == 0xC0U) {
            cp    = b & 0x1FU;
            extra = 1;
        } else if ((b & 0xF0U) == 0xE0U) {
            cp    = b & 0x0FU;
            extra = 2;
        } else if ((b & 0xF8U) == 0xF0U) {
            cp    = b & 0x07U;
            extra = 3;
        } else {
            ++i;
            return REPLACEMENT;
        }
        ++i;
        for (size_t k = 0; k < extra; ++k) {
            if (i >= text.size()) {
                return REPLACEMENT;
            }
            const auto cc = static_cast<unsigned char>(text[i]);
            if ((cc & 0xC0U) != 0x80U) {
                return REPLACEMENT;
            }
            cp = (cp << 6U) | (cc & 0x3FU);
            ++i;
        }
        return cp;
    }

    static uint32_t decodeUtf16(std::u16string_view text, size_t & i)
    {
        const auto unit = static_cast<uint32_t>(text[i]);
        ++i;
        if (unit >= 0xD800U && unit <= 0xDBFFU && i < text.size()) {
            const auto low = static_cast<uint32_t>(text[i]);
            if (low >= 0xDC00U && low <= 0xDFFFU) {
                ++i;
                return 0x10000U + (((unit - 0xD800U) << 10U) | (low - 0xDC00U));
            }
        }
        return unit;
    }

    static uint32_t decodeWide(std::wstring_view text, size_t & i)
    {
        if constexpr (sizeof(wchar_t) == 2) {
            const auto unit = static_cast<uint32_t>(text[i]);
            ++i;
            if (unit >= 0xD800U && unit <= 0xDBFFU && i < text.size()) {
                const auto low = static_cast<uint32_t>(text[i]);
                if (low >= 0xDC00U && low <= 0xDFFFU) {
                    ++i;
                    return 0x10000U + (((unit - 0xD800U) << 10U) | (low - 0xDC00U));
                }
            }
            return unit;
        } else {
            const auto cp = static_cast<uint32_t>(text[i]);
            ++i;
            return cp;
        }
    }

    static void appendCodepointToUtf8(std::string & out, uint32_t cp)
    {
        if (cp < 0x80U) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800U) {
            out.push_back(static_cast<char>(0xC0U | (cp >> 6U)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else if (cp < 0x10000U) {
            out.push_back(static_cast<char>(0xE0U | (cp >> 12U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        } else {
            out.push_back(static_cast<char>(0xF0U | (cp >> 18U)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 12U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | ((cp >> 6U) & 0x3FU)));
            out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
        }
    }

    static void appendCodepointToUtf16(std::u16string & out, uint32_t cp)
    {
        if (cp < 0x10000U) {
            out.push_back(static_cast<char16_t>(cp));
        } else {
            const uint32_t v = cp - 0x10000U;
            out.push_back(static_cast<char16_t>(0xD800U | (v >> 10U)));
            out.push_back(static_cast<char16_t>(0xDC00U | (v & 0x3FFU)));
        }
    }

    static void appendCodepointToWide(std::wstring & out, uint32_t cp)
    {
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp < 0x10000U) {
                out.push_back(static_cast<wchar_t>(cp));
            } else {
                const uint32_t v = cp - 0x10000U;
                out.push_back(static_cast<wchar_t>(0xD800U | (v >> 10U)));
                out.push_back(static_cast<wchar_t>(0xDC00U | (v & 0x3FFU)));
            }
        } else {
            out.push_back(static_cast<wchar_t>(cp));
        }
    }
};

} // namespace Common
