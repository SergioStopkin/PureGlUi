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

#include "ui/color.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"

#include <string>

namespace Ui::Render {

// One batched image (SVG or raster) UiRenderer draws
struct alignas(128) image_op_t final {
    std::string             src;
    Ui::Res::Type::bound_t  pos;
    Ui::Res::Type::border_t radius;
    Ui::Color               tint;
    bool                    isSvg    = false;
    bool                    tinted   = false;
    bool                    hovered  = false;
    bool                    active   = false;
    bool                    isFilled = false; // true draws outline art as a solid shape
};

} // namespace Ui::Render
