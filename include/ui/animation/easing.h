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

namespace Ui::Animation {

// A CSS timing function, cubic-bezier(x1, y1, x2, y2): the curve from (0, 0) to (1, 1)
// through two control points, x the time gone and y the way gone. Both x within 0..1, so
// it never runs back in time; a y past them overshoots. CSS's default, ease, when unset
struct alignas(16) easing_t final {
    fpx_t x1 = 0.25F;
    fpx_t y1 = 0.1F;
    fpx_t x2 = 0.25F;
    fpx_t y2 = 1.0F;

    bool operator==(const easing_t &) const = default;
};

// CSS's keywords for it
inline constexpr easing_t LINEAR { .x1 = 0.0F, .y1 = 0.0F, .x2 = 1.0F, .y2 = 1.0F };
inline constexpr easing_t EASE { .x1 = 0.25F, .y1 = 0.1F, .x2 = 0.25F, .y2 = 1.0F };
inline constexpr easing_t EASE_IN { .x1 = 0.42F, .y1 = 0.0F, .x2 = 1.0F, .y2 = 1.0F };
inline constexpr easing_t EASE_OUT { .x1 = 0.0F, .y1 = 0.0F, .x2 = 0.58F, .y2 = 1.0F };
inline constexpr easing_t EASE_IN_OUT { .x1 = 0.42F, .y1 = 0.0F, .x2 = 0.58F, .y2 = 1.0F };

} // namespace Ui::Animation
