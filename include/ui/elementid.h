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

namespace Ui {

// Element ID range starts, assigned by the resource loader at load time. Each
// authored element (menu button, toolbar button, menu item) gets base+counter
// so a click - which carries only a numeric id - routes back to its actionKey.
//   1000-1999: menu buttons    (MenuBase + counter)
//   2000-2999: toolbar buttons (ButtonBase + counter)
//   5000-5999: menu items      (ItemBase + counter)
//   9997-9999: special elements (tab arrows, status text) - reserved literals
enum class ElementId : id_t {
    MenuBase   = 1000,
    ButtonBase = 2000,
    ItemBase   = 5000,
};

} // namespace Ui
