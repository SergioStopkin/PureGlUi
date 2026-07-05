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

#include "ui/interface/irenderer.h"
#include "ui/interface/iwindow.h"
#include "ui/window/compositetexture.h"

namespace Ui::Window {

// A host-provided content surface embedded in the viewport.
// window carries geometry/visibility/native ops only; pairing is the host's
// window+renderer Connector (an IRenderer) that drives frame production,
// events, resize, apply, and readPixels. composite holds the Wayland
// offscreen->texture cache. Both pointers are non-owning - the host owns them.
struct alignas(128) content_surface_t final {
    CompositeTexture composite;
    Ui::IWindow *    window  = nullptr;
    Ui::IRenderer *  pairing = nullptr;
    bool             isReady = true; // false while the host is mid async-load (skip rendering)
};

} // namespace Ui::Window
