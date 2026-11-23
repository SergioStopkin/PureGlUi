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

namespace Ui::Window {

// A menu's `corners` with the right-hand ones square where an open submenu meets its edge:
// the top one beside its first item (`isTopCut`), the bottom one beside its last
// (`isBottomCut`) - physical radii or CSS border alike
[[nodiscard]] inline Ui::Res::Type::border_t menuCornersOf(Ui::Res::Type::border_t corners,
                                                           bool                    isTopCut,
                                                           bool                    isBottomCut)
{
    if (isTopCut) {
        corners.topRight = 0;
    }
    if (isBottomCut) {
        corners.bottomRight = 0;
    }
    return corners;
}

} // namespace Ui::Window
