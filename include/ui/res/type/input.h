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

namespace Ui::Res::Type {

// UI input behaviour tunables loaded from res/input.json: scrollable-popup
// scrolling, read by the popup/dialog renderers, and how long the tooltip and the
// Ctrl+Tab list wait. The 3D viewport half (zoom/rotation/double-click) lives in
// viewport_t (host).
struct alignas(32) input_t final {
    bool  scrollNatural {};
    fpx_t scrollSpeed { 40.0F };
    fpx_t scrollSmooth { 15.0F };
    fpx_t scrollSnapThreshold { 0.5F };
    int   tooltipDelayMs { 300 };     // a toolbar button's first tooltip waits for the pointer to rest
    int   tabSwitcherDelayMs { 200 }; // Ctrl held this long after Ctrl+Tab before its list goes up

    bool operator==(const input_t &) const = default;
};

} // namespace Ui::Res::Type
