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

#include "ui/res/type/colorpair.h"
#include "ui/type.h"

#include <string>

namespace Ui::Render {

// Everything one element contributes to a frame. Resolved in one pass so the
// tab/menu/button lookups happen once instead of once per attribute; an element
// that draws nothing leaves this default-constructed
struct alignas(128) element_style_t final {
    Ui::Res::Type::color_pair_t colors;
    std::string                 text;     // display label, already truncated to the bound
    std::string                 imageSrc; // icon path, empty when the element has none
    Ui::font_handle_t           font = 0; // 0 = draws no text
    fpx_t                       padH = 0; // horizontal text inset
};

} // namespace Ui::Render
