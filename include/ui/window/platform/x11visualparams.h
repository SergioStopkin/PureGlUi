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

#include "ui/window/platform/x11include.h"

namespace Ui::Window::Platform {

// The visual X11Window picks for a new window, and the parent and colormap it is made with
struct alignas(64) x11_visual_params_t final {
    ::Window      parentWin = 0;
    Colormap      colormap  = 0;
    int           depth     = 0;
    Visual *      visual    = nullptr;
    XVisualInfo * vi        = nullptr; // only set for the main window, must be freed
};

} // namespace Ui::Window::Platform
