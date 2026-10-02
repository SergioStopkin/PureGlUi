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

#include "ui/res/type/layout.h"
#include "ui/type.h"

namespace Ui::Render {

// The workspace tab strip's width in a window `windowWidth` wide, all CSS px: the room
// between the two toolbars
[[nodiscard]] inline fpx_t tabStripWidthOf(const Ui::Res::Type::layout_t & layout, fpx_t windowWidth)
{
    return windowWidth - layout.leftToolbar.width - layout.rightToolbar.width;
}

} // namespace Ui::Render
