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

#include <algorithm>
#include <cstddef>

namespace Ui::Render {

// How many of `count` tabs a strip `stripWidth` wide shows however it is scrolled, all
// CSS px: every one while they fit at their narrowest, else as many as fit between both
// arrows - never fewer than one, the fewest UiLayout::buildWorkspaceTab ever shows
[[nodiscard]] inline std::size_t tabsShownOf(fpx_t stripWidth, fpx_t minTabWidth, fpx_t arrowWidth, std::size_t count)
{
    if (minTabWidth <= 0.0F || minTabWidth * static_cast<fpx_t>(count) <= stripWidth) {
        return count;
    }
    const fpx_t between = stripWidth - (arrowWidth * 2.0F);
    return std::max(static_cast<std::size_t>(std::max(between, 0.0F) / minTabWidth), std::size_t { 1 });
}

} // namespace Ui::Render
