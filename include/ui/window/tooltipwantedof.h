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

// The tooltip the pointer asks for: the hovered toolbar button's (`button`, for
// `hoveredButton`, INVALID_ID for none) unless a press silenced it (spentButtonOf),
// else the one the content surface under the pointer asks for (`content`)
//
// An empty text is none whatever its anchor, `{}`, so it equals the none shown
[[nodiscard]] inline Ui::Window::tooltip_t tooltipWantedOf(const Ui::Window::tooltip_t & button,
                                                           Ui::id_t                      hoveredButton,
                                                           Ui::id_t                      spentButton,
                                                           const Ui::Window::tooltip_t & content)
{
    const Ui::Window::tooltip_t & wanted = (hoveredButton == Ui::INVALID_ID) ? content : button;
    if (wanted.text.empty() || (hoveredButton != Ui::INVALID_ID && hoveredButton == spentButton)) {
        return {};
    }
    return wanted;
}

} // namespace Ui::Window
