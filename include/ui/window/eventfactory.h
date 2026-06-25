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

#include "ui/interface/ieventos.h"

#include <memory>

// Include platform-specific event handler based on build target (compile-time)
#if defined(_WIN32)
#include "ui/window/platform/win32event.h"
#elif defined(__APPLE__)
#include "ui/window/platform/macosevent.h"
#elif defined(HAVE_WAYLAND)
#include "ui/window/platform/waylandevent.h"
#elif defined(HAVE_X11)
#include "ui/window/platform/x11event.h"
#endif

namespace Ui::Window {

// Type alias for the current platform's native event handler (compile-time detection)
#if defined(_WIN32)
using NativeEvent = Platform::Win32Event;
#elif defined(__APPLE__)
using NativeEvent = Platform::MacOsEvent;
#elif defined(HAVE_WAYLAND)
using NativeEvent = Platform::WaylandEvent;
#elif defined(HAVE_X11)
using NativeEvent = Platform::X11Event;
#else
#error "No supported event system found"
#endif

/**
 * @brief Factory for creating platform-specific event handlers
 *
 * Uses compile-time platform detection to instantiate the correct event type.
 */
class EventFactory final {
public:
    /**
     * @brief Create a new platform-specific event handler
     * @return Unique pointer to the native event implementation
     */
    static std::unique_ptr<NativeEvent> create() { return std::make_unique<NativeEvent>(); }
};

} // namespace Ui::Window
