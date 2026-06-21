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

#include "ui/res/type/border.h"
#include "ui/type.h"

namespace Ui::Res::Type {

struct alignas(64) region_t final {
    fpx_t    height {};
    fpx_t    width {};
    fpx_t    margin {};
    fpx_t    padding {};
    fpx_t    top {};
    fpx_t    left {};
    fpx_t    right {};
    fpx_t    bottom {};
    border_t border;

    bool operator==(const region_t &) const = default;
};

} // namespace Ui::Res::Type
