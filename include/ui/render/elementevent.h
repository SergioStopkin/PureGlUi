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

#include "ui/render/eventkind.h"
#include "ui/render/uielement.h"
#include "ui/type.h"

namespace Ui::Render {

// What happened, to which element.
//
// `event` is a single kind, never a mask - EventKind doubles as the accept mask
// on UiElement, so it reads like one.
//
// x/y are SURFACE-local CSS pixels: whatever space the producing renderer was
// handed, main window or popup. Subtract the element bound for element-local.
//
// `changed` is independent of the hit - clearing a stale hover with nothing
// under the cursor is still a repaint.
struct alignas(32) element_event_t final {
    UiElementType type    = UiElementType::MenuButton;
    id_t          id      = INVALID_ID;
    EventKind     event   = EventKind::None;
    fpx_t         x       = 0;
    fpx_t         y       = 0;
    bool          changed = false;

    [[nodiscard]] bool isHit() const { return id != INVALID_ID && event != EventKind::None; }
};

} // namespace Ui::Render
