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
#include "ui/type.h"

#include <cstddef>
#include <functional>

namespace Ui::Res::Type {

struct alignas(16) border_t final {
    fpx_t topLeft {};
    fpx_t topRight {};
    fpx_t bottomRight {};
    fpx_t bottomLeft {};

    [[nodiscard]] border_t scaled(fpx_t factor) const
    {
        return { topLeft * factor, topRight * factor, bottomRight * factor, bottomLeft * factor };
    }

    [[nodiscard]] bool anyNonZero() const
    {
        return topLeft >= 0.5F || topRight >= 0.5F || bottomRight >= 0.5F || bottomLeft >= 0.5F;
    }

    bool operator==(const border_t &) const = default;
};

} // namespace Ui::Res::Type

template <>
struct std::hash<Ui::Res::Type::border_t> { // NOLINT(altera-struct-pack-align)
    std::size_t operator()(const Ui::Res::Type::border_t & border) const
    {
        return Common::hashOf(border.topLeft, border.topRight, border.bottomRight, border.bottomLeft);
    }
};
