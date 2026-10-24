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

#include "ui/window/display.h"

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

namespace Ui::Window {

// The displays as System info lists them, one line each - "eDP-1: 2560x1600, 89.95 Hz,
// primary" - the rate to a hundredth and left out where unknown; "N/A" for none
[[nodiscard]] inline std::string displaysTextOf(const std::vector<display_t> & displays)
{
    if (displays.empty()) {
        return "N/A";
    }
    std::ostringstream text;
    for (const display_t & display : displays) {
        if (&display != &displays.front()) {
            text << '\n';
        }
        text << (display.name.empty() ? "Display" : display.name) << ": " << display.width << 'x' << display.height;
        if (display.refreshRate > 0.0F) {
            text << ", " << std::round(display.refreshRate * 100.0F) / 100.0F << " Hz";
        }
        if (display.isPrimary) {
            text << ", primary";
        }
    }
    return text.str();
}

} // namespace Ui::Window
