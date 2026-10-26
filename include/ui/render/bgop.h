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
#include "ui/res/type/colorpair.h"

namespace Ui::Render {

// One batched background fill UiRenderer draws, with its optional drop shadow
struct alignas(128) bg_op_t final {
    Ui::Res::Type::bound_t      bound;
    Ui::Res::Type::border_t     radius;
    int                         shadowOffsetX = 0;
    int                         shadowOffsetY = 0;
    int                         shadowBlur    = 0;
    bool                        hasShadow     = false;
    Ui::Res::Type::color_pair_t colors;
    Ui::Color                   shadowColor;
};

} // namespace Ui::Render
