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

#include <string>
#include <string_view>

namespace Ui {

// Named Unicode codepoints used as UI chrome (ellipsis in truncated labels,
// leader dots, future bullets / arrows / checkmarks etc). char32_t underlying
// type is portable - wchar_t is 16-bit on Windows and would truncate astral
// codepoints. List sorted by codepoint value so additions slot in naturally.
enum class Codepoint : char32_t {
    TwoDotH   = 0x2025, // two dot (horizontal)
    ThreeDotH = 0x2026, // three dot (horizontal)
};

// Build a wstring carrying a single named codepoint. Delegates to
// Common::Unicode::fromUtf32 so the Windows 16-bit wchar_t case is encoded as a
// surrogate pair correctly; on Linux / macOS this is a single wide char.
[[nodiscard]] inline std::wstring wstr(Codepoint cp)
{
    const auto value = static_cast<char32_t>(cp);
    return Common::Unicode::fromUtf32(std::u32string_view(&value, 1));
}

} // namespace Ui
