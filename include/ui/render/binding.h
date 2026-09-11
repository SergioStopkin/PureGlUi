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

#include "ui/intentkind.h"
#include "ui/render/eventkind.h"
#include "ui/render/renderscope.h"
#include "ui/render/uielement.h"

namespace Ui::Render {

// What an element raises when an event it accepts occurs, and what must repaint
// afterwards. The payload (id, actionKey, arg) is assembled by the caller, which
// is the only part that needs to know the intent's shape.
struct alignas(2) binding_t final {
    IntentKind  intent = IntentKind::EmitAction;
    RenderScope scope  = RenderScope::Chrome;

    bool operator==(const binding_t &) const = default;
};

// The binding for one (type, event) pair. False means this type does not bind
// that event, so nothing happens - which is how a new interactive element is
// added without editing Context.
//
// MenuButton and MenuItem are deliberately ABSENT. Their intent is chosen from
// res data, not from their type: a menu button with an actionKey emits it while
// one without toggles its popup, and a menu item is inert / opens a dialog /
// emits an action depending on its children and dialog block. A (type, event)
// table cannot express that, so Context resolves those two itself.
[[nodiscard]] inline bool defaultBinding(UiElementType type, EventKind event, binding_t & outBinding)
{
    if (event != EventKind::LeftClick) {
        return false;
    }

    switch (type) {
    case UiElementType::Tab: outBinding = { IntentKind::SwitchTab, RenderScope::Layout }; return true;
    case UiElementType::TabClose: outBinding = { IntentKind::CloseTab, RenderScope::Layout }; return true;
    case UiElementType::ToolbarButton: outBinding = { IntentKind::EmitAction, RenderScope::Chrome }; return true;
    case UiElementType::Text:
        // Status-bar text copies itself; the temp status that follows is what
        // actually repaints, so this asks for nothing of its own
        outBinding = { IntentKind::CopyText, RenderScope::None };
        return true;
    case UiElementType::DockRow: outBinding = { IntentKind::ActivateRow, RenderScope::Chrome }; return true;
    case UiElementType::DockExpander:
        // Sits on top of its row, so it wins the click and toggles instead of
        // selecting - the same overlap trick TabClose uses over Tab
        outBinding = { IntentKind::ToggleRow, RenderScope::Chrome };
        return true;
    case UiElementType::MenuButton:
    case UiElementType::MenuItem:
    case UiElementType::Separator:
    case UiElementType::Image:
    // TabArrow scrolls the tab bar, and DockGrip resizes a dock - both are
    // chrome the framework owns, which intentkind.h defines as internal work
    // rather than an intent. DockGrip also binds no LeftClick at all: it is
    // drag-only, and this table is left-click-only.
    // DockSlider is drag-only too, and its value reaches the host through
    // WindowManager's row-value hook rather than an intent: a drag is a
    // continuous stream, not a discrete request. DockScrollbar is drag-only and
    // does not leave the framework at all - which rows are on screen is the
    // dock's own business.
    case UiElementType::DockScrollbar:
    case UiElementType::DockSlider:
    case UiElementType::DockGrip:
    case UiElementType::TabArrow: return false;
    }
    return false;
}

} // namespace Ui::Render
