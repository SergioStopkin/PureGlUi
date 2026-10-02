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
#include "ui/window/tooltip.h"

namespace Ui::Window {

// Whether `wanted` waits for the pointer to rest (input.json tooltip.delayMs): a toolbar
// button's does while none is `shown`; with one up the next follows at once, and the
// content surface's never waits
[[nodiscard]] inline bool isTooltipDelayed(const Ui::Window::tooltip_t & wanted,
                                           Ui::id_t                      hoveredButton,
                                           const Ui::Window::tooltip_t & shown)
{
    return hoveredButton != Ui::INVALID_ID && !wanted.text.empty() && shown.text.empty();
}

} // namespace Ui::Window
