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

#include <cstddef>

namespace Ui {

// The first of `shown` items in view, scrolled the least from `offset` that shows item `index`:
// first when it stands before them, last when after. With none shown nothing past them is
// reached for, so the offset never runs beyond the item
[[nodiscard]] constexpr std::size_t revealedOffsetOf(std::size_t offset, std::size_t index, std::size_t shown)
{
    if (index < offset) {
        return index;
    }
    if (shown > 0 && index >= offset + shown) {
        return index + 1 - shown;
    }
    return offset;
}

} // namespace Ui
