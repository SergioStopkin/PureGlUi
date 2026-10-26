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

namespace Ui::Gl {

// An SVG's tight content box as fractions of its intrinsic size, transparent padding left out
struct alignas(16) content_bounds_t final {
    fpx_t u0 = 0.0F; // left fraction (0-1)
    fpx_t v0 = 0.0F; // top fraction (0-1)
    fpx_t u1 = 1.0F; // right fraction (0-1)
    fpx_t v1 = 1.0F; // bottom fraction (0-1)
};

} // namespace Ui::Gl
