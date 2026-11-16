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

#include "ui/type.h"

#include <cmath>
#include <cstddef>

namespace Ui::Render {

// How many rows `rowHeight` tall a list `height` tall shows, both CSS px; none for rows of
// no height. Rounded: the list is made a whole number of rows tall, and its trip to the
// physical grid and back can leave that a hair short
[[nodiscard]] inline std::size_t rowsShownOf(fpx_t height, fpx_t rowHeight)
{
    return rowHeight > 0.0F ? static_cast<std::size_t>(std::lround(height / rowHeight)) : 0;
}

} // namespace Ui::Render
