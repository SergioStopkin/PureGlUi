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

namespace Ui::Render {

// Types of UI elements (used by renderers, windows, and app)
enum class UiElementType : unsigned char {
    MenuButton,    // Top menu bar button ("File", "Edit", etc.)
    ToolbarButton, // Left/right toolbar icon button
    MenuItem,      // Popup dropdown menu item row
    Separator,     // Popup dropdown separator line
    Text,          // Text-only element (status bar text, labels)
    Image,         // SVG icon image
    Tab,           // Workspace tab
    TabClose,      // Workspace tab close button (X)
    TabArrow,      // Workspace tab scroll arrow ('<' or '>')
    DockRow,       // One line of host-projected dock content
    DockExpander,  // Expand/collapse box on a DockRow that has children
    DockGrip,      // Resize handle on a dock's viewport-facing edge
    DockSlider,    // Draggable track on a DockRow whose kind is Slider
    DockScrollbar  // Scroll track + thumb, when a dock holds more rows than fit
};

inline int toInt(UiElementType type) { return static_cast<int>(type); }

// What a fresh element of this type accepts. addElement() stamps it, and a
// caller that needs something different overrides the mask on the returned
// element - which is how an element stops being a switch case.
//
// These values reproduce the behaviour that was previously spread across
// UiLayout::hitTest, UiRenderer::updateHover and PopupRenderer::onMousePress.
// Text is the notable one: it is clickable (status bar copies on click) but
// deliberately NOT hoverable, which used to be a `type != Text` test.
[[nodiscard]] inline constexpr EventKind defaultAccepts(UiElementType type)
{
    const auto clickAndHover = static_cast<EventKind>(Common::Bit::Or(EventKind::LeftClick, EventKind::Hover));
    switch (type) {
    case UiElementType::MenuButton:
    case UiElementType::ToolbarButton:
    case UiElementType::MenuItem:
    case UiElementType::Tab:
    case UiElementType::TabClose:
    case UiElementType::TabArrow:
    case UiElementType::DockRow:
    case UiElementType::DockExpander: return clickAndHover;
    case UiElementType::Text: return EventKind::LeftClick;
    // Drag only, all three, for their own reasons.
    //
    // Grip hover (the resize cursor) is still resolved geometrically by
    // WindowManager, and two hover paths for one element would fight over the
    // cursor. A press anywhere on a slider track jumps to that spot and keeps
    // dragging, so the whole gesture is a drag with no separate click to bind.
    // A scrollbar element spans the whole track: pressing the thumb drags it,
    // pressing beside it pages, and only the first keeps the pointer.
    case UiElementType::DockGrip:
    case UiElementType::DockSlider:
    case UiElementType::DockScrollbar: return ANY_DRAG;
    case UiElementType::Separator:
    case UiElementType::Image: return EventKind::None;
    }
    return EventKind::None;
}

} // namespace Ui::Render
