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

namespace Ui::Backend::Gl::TextAlign {

// Physical startX for left-aligned text inside `box` with horizontal padding.
[[nodiscard]] inline fpx_t startXLeft(const Ui::Res::Type::bound_t & box, fpx_t paddingHCss, fpx_t scale)
{
    return (box.x + paddingHCss) * scale;
}

// Physical startX for right-aligned text of width `textWidthCss` inside `box`.
[[nodiscard]] inline fpx_t
startXRight(const Ui::Res::Type::bound_t & box, fpx_t textWidthCss, fpx_t paddingHCss, fpx_t scale)
{
    return (box.x + box.w - paddingHCss - textWidthCss) * scale;
}

// Physical startX for centered text of width `textWidthCss` inside `box`.
[[nodiscard]] inline fpx_t startXCenter(const Ui::Res::Type::bound_t & box, fpx_t textWidthCss, fpx_t scale)
{
    return (box.x + (box.w - textWidthCss) / 2.0F) * scale;
}

// Centered, but never starts before `box.x + minPaddingHCss` - prevents long
// labels from overflowing the left edge of a narrow centered box (e.g. tabs,
// menu buttons that should center when there's room and left-align when there
// isn't).
[[nodiscard]] inline fpx_t
startXCenterClamped(const Ui::Res::Type::bound_t & box, fpx_t textWidthCss, fpx_t minPaddingHCss, fpx_t scale)
{
    const fpx_t centered = (box.x + (box.w - textWidthCss) / 2.0F) * scale;
    const fpx_t minX     = (box.x + minPaddingHCss) * scale;
    return centered < minX ? minX : centered;
}

} // namespace Ui::Backend::Gl::TextAlign
