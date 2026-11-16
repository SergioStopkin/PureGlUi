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

namespace Ui::Render {

// Where a row's close button stands in `row`: its icon (`close`'s height) with its margin round
// it, a square `right` in from the row's right edge, centred on the row's height. The Ctrl+Tab
// list's (tab-switcher-close), for drawing and hit-testing both
[[nodiscard]] inline Ui::Res::Type::bound_t rowCloseBoundOf(const Ui::Res::Type::region_t & close,
                                                            const Ui::Res::Type::bound_t &  row)
{
    const fpx_t side = close.height + (close.margin * 2.0F);
    return { .x = row.x + row.w - close.right - side, .y = row.y + ((row.h - side) / 2.0F), .w = side, .h = side };
}

} // namespace Ui::Render
