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

namespace Ui::Res::Type {

struct alignas(16) bound_t final {
    fpx_t x {};
    fpx_t y {};
    fpx_t w {};
    fpx_t h {};

    [[nodiscard]] bool contains(fpx_t px, fpx_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    [[nodiscard]] bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }

    bool operator==(const bound_t &) const = default;
};

} // namespace Ui::Res::Type
