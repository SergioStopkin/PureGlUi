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

#include "ui/res/type/input.h"
#include "ui/type.h"

namespace Ui::Render {

// How many rows `rowHeight` tall a wheel's `deltaY` scrolls, by input.json's scroll speed and
// direction - a dock's and the Ctrl+Tab list's alike: a notch at least one row, however coarse the
// row; none for no turn or rows of no height
[[nodiscard]] inline int rowsScrolledOf(fpx_t deltaY, fpx_t rowHeight, const Ui::Res::Type::input_t & input)
{
    if (deltaY == 0.0F || rowHeight <= 0.0F) {
        return 0;
    }
    const fpx_t pixels = deltaY * (input.scrollNatural ? 1.0F : -1.0F) * input.scrollSpeed;
    const auto  rows   = static_cast<int>(pixels / rowHeight);
    if (rows != 0) {
        return rows;
    }
    return pixels > 0.0F ? 1 : -1;
}

} // namespace Ui::Render
