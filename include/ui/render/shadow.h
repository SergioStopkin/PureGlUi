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
#include "ui/type.h"

namespace Ui::Render {

// Drop-shadow parameters, reused by any drawable that can cast one (rounded
// rects, images). A default-constructed value means "no shadow" (opacity 0),
// so callers that do not want a shadow pass {}. Domain-blind: pure geometry +
// color, no UI state.
struct alignas(32) shadow_t final {
    fpx_t offsetX = 0;
    fpx_t offsetY = 0;
    fpx_t blur    = 0;
    fpx_t opacity = 0; // 0 = no shadow; multiplies color.a()
    Color color;

    [[nodiscard]] bool isVisible() const { return opacity > 0 && color.a() > 0; }

    bool operator==(const shadow_t &) const = default;
};

} // namespace Ui::Render
