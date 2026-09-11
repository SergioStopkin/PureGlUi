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

// Vertical text placement inside its box, read from a "vertical-align" field.
//
// Center is cap-height centred rather than line-box centred - text looks
// centred to the eye when its capitals are, not when its ascender/descender
// bounds are (see font_metrics_t::baselineCap).
enum class AlignV : uint8_t {
    Top = 0,
    Center,
    Bottom,
};

[[nodiscard]] inline AlignV alignVFromName(const std::string & name)
{
    if (name == "top") {
        return AlignV::Top;
    }
    if (name == "bottom") {
        return AlignV::Bottom;
    }
    return AlignV::Center;
}

} // namespace Ui::Res::Type
