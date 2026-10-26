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
#include "ui/window/eventtype.h"
#include "ui/window/keyevent.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mouseevent.h"
#include "ui/window/nativewindowhandle.h"
#include "ui/window/resizeevent.h"
#include "ui/window/scrollevent.h"

#include <string>

namespace Ui::Window {

/**
 * @brief Platform-agnostic window event
 */
struct alignas(128) Event final {
    mouse_event_t      mouse;
    key_event_t        key;
    NativeWindowHandle sourceWindow  = 0;              // Native handle of the window that received the event
    id_t               childWindowId = Ui::INVALID_ID; // Ui::INVALID_ID = main window, otherwise child window ID
    scroll_event_t     scroll;
    resize_event_t     resize;

    EventType type         = EventType::None;
    bool      isPopupEvent = false; // true if event is from popup window

    /**
     * @brief Convert key name and modifiers to shortcut string (e.g., "Ctrl+C", "F1").
     * Platform layer must populate key.name before this is called.
     */
    [[nodiscard]] std::string toShortcutString() const
    {
        if (type != EventType::KeyPress || key.name.at(0) == '\0') {
            return "";
        }

        std::string result;

        // Add modifiers in order: Ctrl, Alt, Shift
        if (key.modifiers.has(KeyModifier::Control)) {
            result += "Ctrl+";
        }
        if (key.modifiers.has(KeyModifier::Alt)) {
            result += "Alt+";
        }
        if (key.modifiers.has(KeyModifier::Shift)) {
            result += "Shift+";
        }

        result += key.name.data();
        return result;
    }
};

} // namespace Ui::Window
