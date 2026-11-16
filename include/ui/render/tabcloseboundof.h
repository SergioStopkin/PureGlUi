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
#include "ui/res/type/layout.h"
#include "ui/type.h"

namespace Ui::Render {

// Where a tab's close glyph stands in `tab`: a square its margin in from the top and the
// bottom, `right` in from the right edge (workspace-tab-close). Layout places it so for
// hit-testing, and drawing so in the tab where it is drawn, gliding or dragged
[[nodiscard]] inline Ui::Res::Type::bound_t tabCloseBoundOf(const Ui::Res::Type::layout_t & layout,
                                                            const Ui::Res::Type::bound_t &  tab)
{
    const fpx_t side = tab.h - (layout.tabCloseMargin * 2.0F);
    return { .x = tab.x + tab.w - side - layout.tabCloseRight,
             .y = tab.y + layout.tabCloseMargin,
             .w = side,
             .h = side };
}

} // namespace Ui::Render
