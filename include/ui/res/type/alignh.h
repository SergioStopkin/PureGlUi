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

namespace Ui::Res::Type {

// Horizontal text placement inside its box. Read from a "text-align" field, so
// the JSON spelling matches the CSS property it mirrors.
//
// CenterClamped centres while there is room and falls back to left-aligned
// rather than overflowing the left edge - what a tab or a menu button needs
// when its label outgrows the box.
enum class AlignH : uint8_t {
    Left = 0,
    Center,
    Right,
    CenterClamped,
};

[[nodiscard]] inline AlignH alignHFromName(const std::string & name)
{
    if (name == "center") {
        return AlignH::Center;
    }
    if (name == "right") {
        return AlignH::Right;
    }
    if (name == "center-clamped") {
        return AlignH::CenterClamped;
    }
    return AlignH::Left;
}

} // namespace Ui::Res::Type
