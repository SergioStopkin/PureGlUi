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

#include "ui/backend/window/iwindow.h"

namespace Ui::Window {

// Hit-test result with coords pre-translated to the content surface's own
// frame. In xwaylandComposite mode the offscreen child receives main-local
// coords, so the renderer would otherwise see out-of-bounds x/y.
struct alignas(16) ContentHit final {
    Ui::Backend::Window::IWindow * window = nullptr;
    int                            lx     = 0;
    int                            ly     = 0;
};

} // namespace Ui::Window
