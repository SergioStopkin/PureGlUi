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

#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

namespace Ui::Render {

// One batched tab loading bar UiRenderer draws, filled sector by sector
struct alignas(64) progress_op_t final {
    Ui::Res::Type::bound_t  tabRect;
    Ui::Res::Type::border_t tabRadius;
    fpx_t                   sectorW      = 0;
    int                     totalSectors = 0;
    int                     progress     = 0;
};

} // namespace Ui::Render
