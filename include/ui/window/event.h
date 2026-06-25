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
#include "ui/window/nativewindowhandle.h"

#include <array>
#include <cstdint>
#include <string>

namespace Ui::Window {

/**
 * @brief Platform-agnostic event types
 */
enum class EventType : unsigned char {
    None,
    CloseRequested,
    Resize,
    MouseButtonPress,
    MouseButtonRelease,
    MouseMove,
    MouseLeave,
    Scroll,
    KeyPress,
    KeyRelease
};

/**
 * @brief Mouse button identifiers
 */
enum class MouseButton : unsigned char { Left = 1, Middle = 2, Right = 3 };

inline int toInt(MouseButton b) { return static_cast<int>(b); }

inline MouseButton toMouseButton(int v) { return static_cast<MouseButton>(v); }

/**
 * @brief Keyboard modifier flags
 */
enum class KeyModifier : uint32_t { None = 0, Shift = 1U << 0U, Control = 1U << 1U, Alt = 1U << 2U, Meta = 1U << 3U };

inline uint32_t toUint(KeyModifier m) { return static_cast<uint32_t>(m); }

inline KeyModifier toKeyModifier(uint32_t v) { return static_cast<KeyModifier>(v); }

inline KeyModifier operator|(KeyModifier a, KeyModifier b) { return toKeyModifier(toUint(a) | toUint(b)); }

inline KeyModifier operator&(KeyModifier a, KeyModifier b) { return toKeyModifier(toUint(a) & toUint(b)); }

inline bool hasModifier(KeyModifier modifiers, KeyModifier flag) { return (toUint(modifiers) & toUint(flag)) != 0; }

/**
 * @brief Platform-agnostic window event
 */
struct alignas(64) Event final {
    // Mouse event data
    struct alignas(16) {
        int         x      = 0;
        int         y      = 0;
        MouseButton button = MouseButton::Left;
        // Number of clicks in the current burst: 1 = single, 2 = double, ...
        // Populated by the platform event layer for MouseButtonPress only;
        // the rest of the codebase can read this on first press of a gesture
        // to react to double-clicks without timing logic of its own.
        int clickCount = 1;
    } mouse;

    // Keyboard event data
    struct alignas(16) {
        uint32_t             keysym    = 0; // Platform-specific key symbol
        KeyModifier          modifiers = KeyModifier::None;
        std::array<char, 8>  text      = {}; // UTF-8 typed character (if available)
        std::array<char, 16> name      = {}; // Logical key name (e.g. "Return", "F1", "q")
    } key;

    NativeWindowHandle sourceWindow  = 0;              // Native handle of the window that received the event
    id_t               childWindowId = Ui::INVALID_ID; // Ui::INVALID_ID = main window, otherwise child window ID

    // Scroll event data
    struct alignas(4) {
        fpx_t deltaY = 0; // positive = scroll down, negative = scroll up
    } scroll;

    // Resize event data
    struct alignas(8) {
        fpx_t width  = 0;
        fpx_t height = 0;
    } resize;

    EventType type         = EventType::None;
    bool      isPopupEvent = false; // true if event is from popup window

    /**
     * @brief Convert key name and modifiers to shortcut string (e.g., "Ctrl+C", "F1").
     * Platform layer must populate key.name before this is called.
     */
    [[nodiscard]] std::string toShortcutString() const
    {
        if (type != EventType::KeyPress || key.name[0] == '\0') {
            return "";
        }

        std::string result;

        // Add modifiers in order: Ctrl, Alt, Shift
        if (hasModifier(key.modifiers, KeyModifier::Control)) {
            result += "Ctrl+";
        }
        if (hasModifier(key.modifiers, KeyModifier::Alt)) {
            result += "Alt+";
        }
        if (hasModifier(key.modifiers, KeyModifier::Shift)) {
            result += "Shift+";
        }

        result += key.name.data();
        return result;
    }
};

} // namespace Ui::Window
