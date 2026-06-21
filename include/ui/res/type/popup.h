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

struct alignas(32) popup_t final {
    fpx_t itemHeight       = {}; // fontSize * lineHeight + padV * 2 (cached)
    fpx_t itemPaddingV     = {};
    fpx_t itemPaddingH     = {};
    fpx_t separatorHeight  = {}; // the line itself
    fpx_t separatorMarginV = {};
    fpx_t separatorMarginH = {};

    bool operator==(const popup_t &) const = default;
};

} // namespace Ui::Res::Type
