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

#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

namespace Ui::Window {

// A pointer event's payload; the ints first and the bytes after, so nothing pads
struct alignas(16) mouse_event_t final {
    int             x          = 0;
    int             y          = 0;
    int             clickCount = 1; // clicks in the burst, 1 single, 2 double; set on a press only
    MouseButton     button     = MouseButton::Left;
    key_modifiers_t modifiers; // held at the press, the set a key event carries too
};

} // namespace Ui::Window
