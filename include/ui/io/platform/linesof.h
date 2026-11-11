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

#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace Ui::Io::Platform {

// What ends each line zenity and kdialog write, one picked path per line
inline constexpr std::string_view LINE_END = "\n";

// The paths a fallback dialog wrote, one a line, the end of the last one dropped: nothing written
// is no path, as a cancel writes nothing
inline std::vector<std::string> linesOf(std::string_view text)
{
    if (text.ends_with(LINE_END)) {
        text.remove_suffix(LINE_END.size());
    }
    std::vector<std::string> lines;
    for (const auto line : std::views::split(text, LINE_END)) {
        lines.emplace_back(line.begin(), line.end());
    }
    return lines;
}

} // namespace Ui::Io::Platform
