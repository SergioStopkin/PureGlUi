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

#include "ui/res/type/bound.h"
#include "ui/window/compositetexture.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/pointershape.h"

#include <functional>
#include <string>

namespace Ui::Window {

// A host-provided content surface embedded in the viewport: the host Connector's pairing,
// which drives its frames, events, resize, apply and readPixels, its window reached through
// it. composite holds the Wayland offscreen->texture cache
struct alignas(128) content_surface_t final {
    CompositeTexture                composite;
    std::reference_wrapper<Pairing> pairing;                // the host's, outliving the registration
    bool                            isReady = true;         // false while the host is mid async-load (skip rendering)
    PointerShape           pointer = PointerShape::Default; // what the pointer shows over it (setContentPointer)
    std::string            tooltipText;   // what its host names under the pointer (setContentTooltip), empty for none
    Ui::Res::Type::bound_t tooltipAnchor; // beside what, in the surface's own physical px

    // A surface is registered by its pairing alone, everything else at its default
    explicit content_surface_t(Pairing & hostPairing)
        : pairing(hostPairing)
    {
    }
};

} // namespace Ui::Window
