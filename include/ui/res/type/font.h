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

#include <functional>
#include <string>

namespace Ui::Res::Type {

enum class FontWeight : unsigned char {
    Regular = 0,
    Bold,
};

struct alignas(64) font_t final {
    std::string family;
    int         size   = 0;
    FontWeight  weight = FontWeight::Regular;

    bool operator==(const font_t &) const = default;
};

} // namespace Ui::Res::Type

template <>
struct std::hash<Ui::Res::Type::font_t> { // NOLINT(altera-struct-pack-align)
    std::size_t operator()(const Ui::Res::Type::font_t & font) const
    {
        return std::hash<std::string>()(font.family) ^ (std::hash<int>()(font.size) << 1U)
             ^ (std::hash<int>()(static_cast<int>(font.weight)) << 2U);
    }
};
