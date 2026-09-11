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

namespace Ui {

// Element ID range starts, assigned by the resource loader at load time. Each
// authored element (menu button, toolbar button, menu item) gets base+counter
// so a click - which carries only a numeric id - routes back to its actionKey.
//   1000-1999: menu buttons    (MenuBase + counter)
//   2000-2999: toolbar buttons (ButtonBase + counter)
//   5000-5999: menu items      (ItemBase + counter)
//   9997-9999: special elements (tab arrows, status text)
//     90000+ : dock grips      (DockGripBase + dock id)
//     95000+ : dock scrollbars (DockScrollBase + dock id)
//    100000+ : dock rows       (DockRowBase + host row id)
//
// One space for the whole system: an id names one element, so the ranges never
// overlap. DockRowBase is open-ended and sits far above the rest because dock
// content is host-projected - the host supplies the row id, the framework only
// offsets it
enum class ElementId : id_t {
    MenuBase       = 1000,
    ButtonBase     = 2000,
    ItemBase       = 5000,
    DockGripBase   = 90'000,
    DockScrollBase = 95'000,
    DockRowBase    = 100'000,
};

inline constexpr id_t TAB_ARROW_LEFT  = 9997;
inline constexpr id_t TAB_ARROW_RIGHT = 9998;
inline constexpr id_t STATUS_TEXT_ID  = 9999;

// A dock id into the grip range, and back. Grips are framework-owned (one per
// dock, id from m_docks) rather than host-projected, so unlike a dock row this
// offset never crosses the host boundary
[[nodiscard]] constexpr id_t toDockGripElementId(id_t dockId)
{
    return static_cast<id_t>(ElementId::DockGripBase) + dockId;
}

[[nodiscard]] constexpr id_t toDockIdFromGrip(id_t elementId)
{
    return elementId - static_cast<id_t>(ElementId::DockGripBase);
}

// A dock scrollbar needs a range of its own rather than sharing the grip's: one
// id names one element
[[nodiscard]] constexpr id_t toDockScrollElementId(id_t dockId)
{
    return static_cast<id_t>(ElementId::DockScrollBase) + dockId;
}

[[nodiscard]] constexpr id_t toDockIdFromScroll(id_t elementId)
{
    return elementId - static_cast<id_t>(ElementId::DockScrollBase);
}

// A host row id into the reserved range, and back. The offset is applied where
// the element is created and removed where the intent is built, so a host never
// sees it and cannot forget it.
[[nodiscard]] constexpr id_t toDockRowElementId(id_t rowId)
{
    return static_cast<id_t>(ElementId::DockRowBase) + rowId;
}

[[nodiscard]] constexpr id_t toDockRowId(id_t elementId)
{
    return elementId - static_cast<id_t>(ElementId::DockRowBase);
}

} // namespace Ui
