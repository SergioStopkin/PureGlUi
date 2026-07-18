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

// Lightweight header: only the native window handle type per platform.
// Does NOT include platform window classes to avoid circular dependencies
// with interface headers (iwindow.h, icontext.h, ieventos.h).

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#ifdef __OBJC__
@class NSView;
#else
using NSView = void;
#endif
#elif defined(HAVE_WAYLAND)
#include <wayland-client.h>
#elif defined(HAVE_X11)
#include "ui/window/platform/x11include.h"
#endif

#include "ui/type.h"

#include <functional>

namespace Ui::Window {

#if defined(_WIN32)
using NativeWindowHandle = HWND;
#elif defined(__APPLE__)
// NSView*: the native drawable for a content surface, also used by popups
// (they resolve the parent NSWindow via [view window]).
using NativeWindowHandle = NSView *;
#elif defined(HAVE_WAYLAND)
using NativeWindowHandle = wl_surface *;
#elif defined(HAVE_X11)
using NativeWindowHandle = ::Window;
#else
#error "No supported window system found"
#endif

// Content-surface event routing: resolve a native window handle to its child
// window id (Ui::INVALID_ID = the main window). WindowManager owns the mapping
// (its content-surface registry) and supplies this; event peers call it at poll
// time to stamp event.childWindowId.
using child_id_fn_t = std::function<id_t(NativeWindowHandle)>;

} // namespace Ui::Window
