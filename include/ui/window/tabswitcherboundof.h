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
#include "ui/type.h"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace Ui::Window {

// Where the Ctrl+Tab list stands, all main-window CSS px: `width` wide over the window's
// middle, as many of its `count` rows `rowHeight` tall as fit `margin` clear of the window's
// edges, never fewer than one. None with no row or rows of no height: no list, never a
// window of no height
[[nodiscard]] inline std::optional<Ui::Res::Type::bound_t>
tabSwitcherBoundOf(fpx_t windowWidth, fpx_t windowHeight, fpx_t width, fpx_t margin, fpx_t rowHeight, std::size_t count)
{
    if (rowHeight <= 0.0F || count == 0) {
        return std::nullopt;
    }
    const auto  fitting = static_cast<std::size_t>(std::max((windowHeight - (margin * 2)) / rowHeight, 0.0F));
    const fpx_t height  = static_cast<fpx_t>(std::clamp<std::size_t>(fitting, 1, count)) * rowHeight;
    return Ui::Res::Type::bound_t { .x = (windowWidth - width) / 2,
                                    .y = (windowHeight - height) / 2,
                                    .w = width,
                                    .h = height };
}

} // namespace Ui::Window
