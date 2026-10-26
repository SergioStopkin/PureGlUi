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

#include "ui/color.h"
#include "ui/res/type/alignh.h"
#include "ui/res/type/alignv.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <string>

namespace Ui::Render {

// One batched line of text UiRenderer draws
struct alignas(128) text_op_t final {
    std::string            text;
    Ui::font_handle_t      hFont = 0;
    Ui::Color              color;
    Ui::Res::Type::bound_t pos;
    Ui::Res::Type::AlignH  alignH  = Ui::Res::Type::AlignH::Left;
    Ui::Res::Type::AlignV  alignV  = Ui::Res::Type::AlignV::Center;
    fpx_t                  minPadH = 0; // minimum horizontal padding (CSS px)
};

} // namespace Ui::Render
