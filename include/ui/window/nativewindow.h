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

// Include platform-specific window based on build target (compile-time)
#ifdef _WIN32
#include "ui/window/platform/win32window.h"
#elifdef __APPLE__
#include "ui/window/platform/macoswindow.h"
#elifdef HAVE_WAYLAND
#include "ui/window/platform/waylandwindow.h"
#elifdef HAVE_X11
#include "ui/window/platform/x11window.h"
#endif

namespace Ui::Window {

// Type alias for the current platform's native window (compile-time detection)
#ifdef _WIN32
using NativeWindow = Platform::Win32Window;
#elifdef __APPLE__
using NativeWindow = Platform::MacOsWindow;
#elifdef HAVE_WAYLAND
using NativeWindow = Platform::WaylandWindow;
#elifdef HAVE_X11
using NativeWindow = Platform::X11Window;
#else
#error "No supported window system found"
#endif

} // namespace Ui::Window
