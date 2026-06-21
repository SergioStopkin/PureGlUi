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

#include "ui/backend/window/event.h"
#include "ui/backend/window/iwindow.h"
#include "ui/backend/window/nativewindowhandle.h"
#include "ui/type.h"

#include <cstdint>
#include <string>

namespace Ui::Backend::Window {

/**
 * @brief Abstract interface for OS-level event polling
 *
 * Provides platform-agnostic event polling interface.
 * Implemented by platform-specific event classes (X11Event, Win32Event, MacOsEvent, WaylandEvent).
 * Does NOT own windows - receives them as parameters. Self-rooted (own virtual dtor).
 */
class IEventOS {
public:
    virtual ~IEventOS() = default;

    /**
     * @brief Initialize the event handler with the main window
     * @param window Main window to extract platform-specific data from
     */
    virtual void init(IWindow & window) = 0;

    /**
     * @brief Register a child window for event routing
     */
    virtual void registerChildWindow(id_t id, Ui::Backend::Window::NativeWindowHandle handle) = 0;

    /**
     * @brief Unregister a child window
     */
    virtual void unregisterChildWindow(Ui::Backend::Window::NativeWindowHandle handle) = 0;

    /**
     * @brief Register a popup-type window for event routing (menu, submenu, dialog)
     */
    virtual void addPopupWindow(Ui::Backend::Window::NativeWindowHandle handle) = 0;

    /**
     * @brief Unregister a popup-type window
     */
    virtual void removePopupWindow(Ui::Backend::Window::NativeWindowHandle handle) = 0;

    /**
     * @brief Check if there are pending events
     * @param window Window to check for events
     * @return true if events are waiting to be processed
     */
    [[nodiscard]] virtual bool hasPendingEvents(IWindow & window) const = 0;

    /**
     * @brief Poll for the next event (non-blocking)
     * @param event Output parameter filled with event data
     * @param window Window to poll events from
     * @return true if an event was retrieved, false if no events pending
     */
    virtual bool pollEvent(Ui::Backend::Window::Event & event, IWindow & window) = 0;

    /**
     * @brief Flush pending output to the display server
     * @param window Window to flush
     */
    virtual void flush(IWindow & window) = 0;

    /**
     * @brief Copy text to system clipboard
     * @param text Text to copy
     * @param window Main window (used for selection ownership)
     * @return true on success
     */
    virtual bool copyToClipboard(IWindow & window, const std::string & text) = 0;

    /**
     * @brief Configure double-click detection thresholds (loaded from
     * res/input.json by ResManager). No-op on platforms where the OS
     * supplies a native click count (e.g. macOS NSEvent.clickCount).
     */
    virtual void setDoubleClickConfig(uint32_t /*intervalMs*/, int /*distancePx*/) { }
};

} // namespace Ui::Backend::Window
