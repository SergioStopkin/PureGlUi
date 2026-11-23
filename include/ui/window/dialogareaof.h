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

#include "ui/config.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/layout.h"
#include "ui/type.h"

#include <algorithm>

namespace Ui::Window {

// Where a dialog may sit in a main window `windowWidth` by `windowHeight` physical px, CSS
// px: inside the chrome (menu+tabs, toolbars, status bar) less its own margin. Docks are
// deliberately not subtracted - a dialog is modal over the whole app, not a guest of the
// workspace they frame.
[[nodiscard]] inline Ui::Res::Type::bound_t dialogAreaOf(const Ui::Res::Type::layout_t & layout,
                                                         fpx_t                           windowWidth,
                                                         fpx_t                           windowHeight)
{
    const fpx_t margin = layout.dialog.margin;
    const fpx_t left   = layout.leftToolbar.width + margin;
    const fpx_t right  = layout.rightToolbar.width + margin;
    const fpx_t top    = layout.topMenu.height + layout.workspaceTab.height + margin;
    const fpx_t bottom = layout.statusBar.height + margin;
    return { .x = left,
             .y = top,
             .w = std::max(1.0F, toCss(windowWidth) - left - right),
             .h = std::max(1.0F, toCss(windowHeight) - top - bottom) };
}

} // namespace Ui::Window
