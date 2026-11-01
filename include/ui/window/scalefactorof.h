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

#include "ui/convert.h"
#include "ui/type.h"

#include <cmath>
#include <string>
#include <string_view>

namespace Ui::Window {

// The environment variable scaleFactorOf reads
inline constexpr std::string_view SCALE_VARIABLE = "PUREGLUI_SCALE";

// The factor PUREGLUI_SCALE puts on the OS's DPI, as Qt's QT_SCALE_FACTOR does: a finite
// positive number, else 1, so an unset or mistyped value leaves the OS's scale as it is
[[nodiscard]] inline fpx_t scaleFactorOf(const std::string & text)
{
    const fpx_t factor = Ui::Convert::parseCssNumber(text);
    return (std::isfinite(factor) && factor > 0.0F) ? factor : 1.0F;
}

} // namespace Ui::Window
