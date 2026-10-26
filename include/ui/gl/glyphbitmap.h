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

#include <vector>

namespace Ui::Gl {

// One rasterised glyph FontRenderer caches, with where it sits in its font's atlas
struct alignas(64) glyph_bitmap_t final {
    fpx_t                      width   = 0;
    fpx_t                      height  = 0;
    int                        left    = 0; // bitmap offset
    int                        top     = 0; // bitmap offset
    int                        advance = 0; // in pixels (approx)
    fpx_t                      u0      = 0; // atlas texture coordinates
    fpx_t                      v0      = 0;
    fpx_t                      u1      = 0;
    fpx_t                      v1      = 0;
    std::vector<unsigned char> buf; // grayscale 8bit
};

} // namespace Ui::Gl
