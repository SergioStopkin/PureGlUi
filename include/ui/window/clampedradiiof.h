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

#include "ui/res/type/border.h"
#include "ui/type.h"

#include <algorithm>
#include <cmath>

namespace Ui::Window {

// `radii` rounded to whole px, none past half the shorter side of a `width` by `height` rect
[[nodiscard]] inline Ui::Res::Type::border_t clampedRadiiOf(const Ui::Res::Type::border_t & radii,
                                                            fpx_t                           width,
                                                            fpx_t                           height)
{
    const fpx_t maxR = std::min(width, height) / 2;
    return { .topLeft     = std::min(maxR, std::round(radii.topLeft)),
             .topRight    = std::min(maxR, std::round(radii.topRight)),
             .bottomRight = std::min(maxR, std::round(radii.bottomRight)),
             .bottomLeft  = std::min(maxR, std::round(radii.bottomLeft)) };
}

} // namespace Ui::Window
