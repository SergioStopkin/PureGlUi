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

#include "common/hash.h"

#include <cstddef>
#include <functional>
#include <string>

namespace Ui::Gl {

// One SVG rasterised at one size, as SvgRenderer's texture cache keys it
struct alignas(64) cache_key_t final {
    std::string svgId;
    int         width;
    int         height;

    bool operator==(const cache_key_t & other) const
    {
        return svgId == other.svgId && width == other.width && height == other.height;
    }
};

} // namespace Ui::Gl

template <>
struct std::hash<Ui::Gl::cache_key_t> { // NOLINT(altera-struct-pack-align)
    std::size_t operator()(const Ui::Gl::cache_key_t & key) const
    {
        return Common::hashOf(key.svgId, key.width, key.height);
    }
};
