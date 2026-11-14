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
#include "ui/tabbar.h"
#include "ui/type.h"

#include <optional>

namespace Ui::Window {

// Where a tab dragged along the strip goes as the drag draws it with its left edge at `left`
// (main-window CSS px), Chrome's live reorder: its right edge `swapAt` of the way over its
// right neighbour (0..1, a share of that one's width - at 0.5 its middle), right before the
// tab after that one (INVALID_ID for the end); its left edge as far over its left neighbour,
// right before that one; else none - it stays. Measured on the tab, not the pointer, so where
// it was grabbed does not move the swap.
//
// The neighbours are the strip's order now, while `boundOf` (where a tab is drawn) may still be
// the last layout's: after a jump not redrawn yet, the tab passed stands on the wrong side of
// the dragged one's place, and one on the wrong side is never passed - so a jump is neither made
// twice nor taken back. A neighbour off the strip (scrolled away, w 0) is never passed either
template <typename BoundOf>
[[nodiscard]] std::optional<id_t>
tabMoveOf(const Ui::TabBar & tabBar, const BoundOf & boundOf, id_t draggedId, fpx_t left, fpx_t swapAt)
{
    const Ui::Res::Type::bound_t place = boundOf(draggedId);
    if (const std::optional<id_t> rightId = tabBar.rightIdOf(draggedId); rightId.has_value()) {
        const Ui::Res::Type::bound_t rightTab = boundOf(rightId.value());
        if (rightTab.w > 0 && rightTab.x > place.x && left + place.w > rightTab.x + (rightTab.w * swapAt)) {
            return tabBar.rightIdOf(rightId.value()).value_or(INVALID_ID);
        }
    }
    if (const std::optional<id_t> leftId = tabBar.leftIdOf(draggedId); leftId.has_value()) {
        const Ui::Res::Type::bound_t leftTab = boundOf(leftId.value());
        if (leftTab.w > 0 && leftTab.x < place.x && left < leftTab.x + (leftTab.w * (1.0F - swapAt))) {
            return leftId;
        }
    }
    return std::nullopt;
}

} // namespace Ui::Window
