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

namespace Ui::Window {

// The toolbar button whose tooltip a press silenced, once the pointer has moved to
// (`x`, `y`, main-window CSS px): still it while the pointer stands on it
// (`spentBound`), none once off it
//
// Where the pointer is, never the button's hover: the chrome clears that for its own
// reasons - a dialog opening from the button - with the pointer still on it
[[nodiscard]] inline Ui::id_t
spentButtonOf(Ui::id_t spentButton, const Ui::Res::Type::bound_t & spentBound, fpx_t x, fpx_t y)
{
    return spentBound.contains(x, y) ? spentButton : Ui::INVALID_ID;
}

} // namespace Ui::Window
