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
#include "ui/type.h"

namespace Ui::Res::Type {

// Layout config for the theme preview swatch shown in the themes submenu.
// Loaded from layout.json's "theme-preview" block. Per-theme colors are
// stored separately in ResManager (see themePreviewColors).
struct alignas(32) theme_preview_t final {
    fpx_t                   width  = 20;
    fpx_t                   height = 14;
    Ui::Res::Type::border_t border;
    fpx_t                   right      = 10; // distance from item's right edge
    fpx_t                   splitAngle = 45; // degrees CCW; 45 = "/" line

    bool operator==(const theme_preview_t &) const = default;
};

} // namespace Ui::Res::Type
