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

#include "common/unicode.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace Ui {

// Named Unicode codepoints used as UI chrome (ellipsis in truncated labels,
// leader dots, future bullets / arrows / checkmarks etc), sorted by value so
// additions slot in naturally
//
// Every one listed is in the Basic Multilingual Plane, so 16 bits hold it; an
// astral one does not compile until the type is widened to char32_t
enum class Codepoint : std::uint16_t {
    TwoDotH   = 0x2025, // two dot (horizontal)
    ThreeDotH = 0x2026, // three dot (horizontal)
};

// Build a wstring carrying a single named codepoint. Delegates to
// Common::Unicode::fromUtf32 as char32_t, since wchar_t is 16-bit on Windows and an
// astral codepoint there is a surrogate pair; on Linux / macOS it is one wide char
[[nodiscard]] inline std::wstring wstr(Codepoint cp)
{
    const auto value = static_cast<char32_t>(cp);
    return Common::Unicode::fromUtf32(std::u32string_view(&value, 1));
}

} // namespace Ui
