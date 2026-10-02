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

#include <cstdint>
#include <type_traits>

namespace Ui {

// What an id names, in its top byte, over a 56-bit serial (idOf). One space for
// every id in the app, framework and host alike: element, window, pubsub source
// and subscriber, render-queue entry. A serial cannot run into another kind, so
// no two objects share an id however long the app runs
//
// A running serial is never handed back (Popup's): a stale id then names nothing,
// rather than whatever was made after it
//
// A kind numbered by position in res (Dock, MenuButton, ToolbarButton, MenuItem)
// starts over on every load, so its id holds only until the next reload - what must
// outlive one is kept by key (Shell::reloadChrome reopens a menu that way)
enum class IdKind : std::uint8_t {
    Untagged      = 0, // a raw number that never went through idOf, never assigned
    MainWindow    = 1,
    Popup         = 2, // a menu popup, submenu, dialog or tooltip - a serial per open
    Dock          = 3, // + dock index in the parsed res
    Event         = 4, // + Ui::Window::EventType, a pubsub source
    App           = 5, // the shell, as a pubsub subscriber
    MenuButton    = 6, // + index in res order
    ToolbarButton = 7, // + index in res order
    MenuItem      = 8, // + index in res order, submenu items included
    TabArrow      = 9, // 0 left, 1 right
    StatusText    = 10,
    DockGrip      = 11,  // + dock index
    DockScroll    = 12,  // + dock index
    FrameworkLast = 127, // the last kind the framework may take
    HostFirst     = 128, // a host numbers its own kinds from here
    HostLast      = 254, // the last kind a host may take
    Invalid       = 255, // INVALID_ID's
};

static_assert(sizeof(id_t) == 8, "an id is a kind byte over a 56-bit serial");

inline constexpr id_t ID_KIND_SHIFT = 56;
inline constexpr id_t SERIAL_LAST   = (id_t { 1 } << ID_KIND_SHIFT) - 1; // the last serial of any kind

// Any kind enum, so a host builds its ids here too; the serial may be an enum
// of its own (an EventType)
template <typename Kind, typename Serial>
    requires std::is_enum_v<Kind>
[[nodiscard]] constexpr id_t idOf(Kind kind, Serial serial)
{
    return (static_cast<id_t>(kind) << ID_KIND_SHIFT) | static_cast<id_t>(serial);
}

template <typename Kind = IdKind>
    requires std::is_enum_v<Kind>
[[nodiscard]] constexpr Kind kindOf(id_t id)
{
    return static_cast<Kind>(id >> ID_KIND_SHIFT);
}

[[nodiscard]] constexpr id_t serialOf(id_t id) { return id & SERIAL_LAST; }

// What the framework takes from a host is used as it is, so it has to carry one
// of the host's kinds - two untagged ids a host hands over (a tab's and a
// dock row's) could be the same number
[[nodiscard]] constexpr bool isHostId(id_t id)
{
    return kindOf(id) >= IdKind::HostFirst && kindOf(id) <= IdKind::HostLast;
}

inline constexpr id_t MAIN_WINDOW_ID  = idOf(IdKind::MainWindow, 0);
inline constexpr id_t APP_ID          = idOf(IdKind::App, 0);
inline constexpr id_t TAB_ARROW_LEFT  = idOf(IdKind::TabArrow, 0);
inline constexpr id_t TAB_ARROW_RIGHT = idOf(IdKind::TabArrow, 1);
inline constexpr id_t STATUS_TEXT_ID  = idOf(IdKind::StatusText, 0);

// A dock's grip and scrollbar carry its index, so either maps back to the dock
[[nodiscard]] constexpr id_t toDockGripElementId(id_t dockId) { return idOf(IdKind::DockGrip, serialOf(dockId)); }

[[nodiscard]] constexpr id_t toDockScrollElementId(id_t dockId) { return idOf(IdKind::DockScroll, serialOf(dockId)); }

[[nodiscard]] constexpr id_t toDockId(id_t elementId) { return idOf(IdKind::Dock, serialOf(elementId)); }

} // namespace Ui
