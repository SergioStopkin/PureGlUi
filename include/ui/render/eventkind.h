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

#include "common/flags.h"

#include <cstdint>

namespace Ui::Render {

// What can happen to a UI element. Its set, event_kinds_t, is the per-element capability
// mask: an element declares the set it accepts, and anything outside that set never reaches it
//
// This is what replaces asking "what type is this element?" in four separate
// switches. A new interactive element declares its mask instead.
enum class EventKind : uint16_t {
    None        = 0,
    LeftClick   = 0x0001,
    RightClick  = 0x0002,
    MiddleClick = 0x0004,
    DoubleClick = 0x0008,
    Hover       = 0x0010,
    DragStart   = 0x0020, // press that begins a capture
    Drag        = 0x0040, // motion while captured
    DragEnd     = 0x0080, // release that ends a capture
    Scroll      = 0x0100,
};

// The events an element accepts
using event_kinds_t = Common::flags_t<EventKind>;

// Every click flavour, for an element that wants presses without caring which
// button - and so a mask can be written without spelling out four constants
inline constexpr event_kinds_t ANY_CLICK { EventKind::LeftClick,
                                           EventKind::RightClick,
                                           EventKind::MiddleClick,
                                           EventKind::DoubleClick };

inline constexpr event_kinds_t ANY_DRAG { EventKind::DragStart, EventKind::Drag, EventKind::DragEnd };

} // namespace Ui::Render
