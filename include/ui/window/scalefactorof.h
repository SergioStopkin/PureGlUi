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

#include "ui/type.h"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Ui::Window {

// The environment variable scaleFactorOf reads
inline constexpr std::string_view SCALE_VARIABLE = "PUREGLUI_SCALE";

// The factors it may hold: far below them a DPI rounds to 0, which every toCss divides by,
// and far above them it overflows its cast
inline constexpr fpx_t LEAST_SCALE_FACTOR = 0.25F;
inline constexpr fpx_t MOST_SCALE_FACTOR  = 8.0F;

// The factor PUREGLUI_SCALE puts on the OS's DPI, as Qt's QT_SCALE_FACTOR does: the whole text
// a number in that range, else 1, so an unset or mistyped value leaves the OS's scale as it
// is. NaN compares false, so it is no factor either
[[nodiscard]] inline fpx_t scaleFactorOf(const std::string & text)
{
    try {
        std::size_t read   = 0;
        const fpx_t factor = std::stof(text, &read);
        return (read == text.size() && factor >= LEAST_SCALE_FACTOR && factor <= MOST_SCALE_FACTOR) ? factor : 1.0F;
    } catch (const std::logic_error &) {
        return 1.0F;
    }
}

} // namespace Ui::Window
