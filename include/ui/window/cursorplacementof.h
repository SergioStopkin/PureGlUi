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

#include "ui/window/cursorplacement.h"

#include <cstdlib>

namespace Ui::Window {

// The cursor's size in surface px: the theme is asked for it times the scale
constexpr int CURSOR_SIZE = 24;

// Where a cursor image loaded at themeScale stands on its surface: at the scale up to it that
// stands it nearest CURSOR_SIZE, the smaller of two as near, its hotspot on the pointer.
// A theme without the size asked for hands its nearest, which at themeScale would stand too
// small and at 1 too large; a buffer whose size does not divide by its scale is a protocol
// error, so only a scale both sides divide by is taken. A scale below 1 stays at 1
[[nodiscard]] inline Ui::Window::cursor_placement_t
cursorPlacementOf(int width, int height, int hotspotX, int hotspotY, int themeScale)
{
    int scale = 1;
    for (int candidate = 2; candidate <= themeScale; ++candidate) {
        if (width % candidate == 0 && height % candidate == 0
            && std::abs((width / candidate) - CURSOR_SIZE) < std::abs((width / scale) - CURSOR_SIZE)) {
            scale = candidate;
        }
    }
    return { .bufferScale = scale, .hotspotX = hotspotX / scale, .hotspotY = hotspotY / scale };
}

} // namespace Ui::Window
