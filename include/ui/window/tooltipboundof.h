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
#include "ui/res/type/region.h"
#include "ui/type.h"

#include <algorithm>

namespace Ui::Window {

// Where a tooltip `width` wide stands, all in main-window CSS px: `box.margin` off
// its anchor - after it, or before it where after would run past the window's
// edge, as on the right toolbar - centred on it, `box.height` tall
//
// Never cut off at the window's edge, however narrow the window or low the
// anchor; with no room at all, pinned to the top-left rather than past it
[[nodiscard]] inline Ui::Res::Type::bound_t tooltipBoundOf(const Ui::Res::Type::bound_t &  anchor,
                                                           const Ui::Res::Type::region_t & box,
                                                           fpx_t                           width,
                                                           fpx_t                           windowWidth,
                                                           fpx_t                           windowHeight)
{
    const fpx_t after  = anchor.x + anchor.w + box.margin;
    const fpx_t beside = (after + width > windowWidth) ? anchor.x - box.margin - width : after;
    const fpx_t x      = std::clamp(beside, 0.0F, std::max(0.0F, windowWidth - width));
    const fpx_t y      = std::clamp(anchor.y + ((anchor.h - box.height) / 2.0F),
                               0.0F,
                               std::max(0.0F, windowHeight - box.height));
    return { x, y, width, box.height };
}

} // namespace Ui::Window
