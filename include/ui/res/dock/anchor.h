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

namespace Ui::Res::Dock {

// Screen edge a dock anchors against. Topology beyond Left/Right (stacked
// docks, multi-column groupings) is expressed via dock_config_t::order, not
// via additional anchor values.
enum class DockAnchor : uint8_t {
    Left,
    Right,
};

inline DockAnchor dockAnchorFromName(const std::string & name)
{
    if (name == "right") {
        return DockAnchor::Right;
    }
    return DockAnchor::Left;
}

inline std::string dockAnchorToName(DockAnchor anchor)
{
    switch (anchor) {
    case DockAnchor::Right: return "right";
    case DockAnchor::Left:
    default: return "left";
    }
}

} // namespace Ui::Res::Dock
