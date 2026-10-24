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
#include "ui/window/display.h"

#include <vector>

namespace Ui::Window {

// The rate frames are paced at: the primary display's, or with none primary or no rate
// of its own the first that has one; 0 where none does, which keeps the 60 Hz default
[[nodiscard]] inline fpx_t refreshRateOf(const std::vector<display_t> & displays)
{
    fpx_t rate = 0.0F;
    for (const display_t & display : displays) {
        if (display.refreshRate > 0.0F && (rate == 0.0F || display.isPrimary)) {
            rate = display.refreshRate;
        }
        if (display.refreshRate > 0.0F && display.isPrimary) {
            break;
        }
    }
    return rate;
}

} // namespace Ui::Window
