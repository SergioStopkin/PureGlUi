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

// Lightweight header: only the native display handle type per platform.
// Does NOT include platform window classes to avoid circular dependencies
// with interface headers (iwindow.h, icontext.h).

#if defined(_WIN32)
// Win32 has no display concept; use void* (always nullptr)
#elif defined(__APPLE__)
// macOS has no display concept; use void* (always nullptr)
#elif defined(HAVE_WAYLAND)
#include <wayland-client.h>
#elif defined(HAVE_X11)
#include "ui/window/platform/x11include.h"
#endif

namespace Ui::Window {

#if defined(_WIN32)
using NativeDisplayHandle = void *;
#elif defined(__APPLE__)
using NativeDisplayHandle = void *;
#elif defined(HAVE_WAYLAND)
using NativeDisplayHandle = wl_display *;
#elif defined(HAVE_X11)
using NativeDisplayHandle = Display *;
#else
#error "No supported window system found"
#endif

} // namespace Ui::Window
