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

#include <string>

namespace Common {

// Build a std::string from a NUL-terminated unsigned-char C string - e.g. a
// C-API byte buffer such as Fontconfig's FcChar8* (= unsigned char*). Uses
// std::string's iterator-range constructor, which copies each byte to char
// element-wise, so no pointer cast (reinterpret_cast) is needed to read
// unsigned char as char.
[[nodiscard]] inline std::string fromCString(const unsigned char * str)
{
    const unsigned char * end = str;
    while (*end != 0) {
        // C-string length walk: ++ on the pointer is the only form. clang-tidy-16
        // lacks the check's AllowIncrementDecrementOperators option that sanctions it.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ++end;
    }
    return { str, end };
}

} // namespace Common
