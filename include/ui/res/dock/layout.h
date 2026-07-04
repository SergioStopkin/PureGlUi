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

#include <string>

namespace Ui::Res::Dock {

// Shared dimensional/interaction tunables for every dock instance. Per-dock
// state (current width, expanded flag) lives in dock_state_t and is session
// persisted; the values here come from layout.json once at load time and
// apply uniformly to all docks.
//
// No min/max width bounds: floor is 0 (clamped in DockColumn::onMouseMove),
// ceiling is the dynamic viewport-available space (clamped by WindowManager).
struct alignas(64) dock_layout_t final {
    fpx_t       gripWidth {};      // grip-strip width (Inkscape-style 3-dot handle)
    fpx_t       gripRadius {};     // grip corner radius on the viewport-facing edge (0 = sharp)
    fpx_t       clickThreshold {}; // drag distance under this counts as a click, not a resize
    std::string gripIcon;          // SVG filename rendered on every grip strip (e.g. "22EE.svg")

    bool operator==(const dock_layout_t &) const = default;
};

} // namespace Ui::Res::Dock
