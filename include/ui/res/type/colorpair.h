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

namespace Ui::Res::Type {

struct alignas(8) color_pair_t final {
    Ui::Color fg;
    Ui::Color bg;

    bool operator==(const color_pair_t &) const = default;
};

inline Ui::Color inheritBg(const Ui::Color & bg, const Ui::Color & parentBg) { return bg.isInherit() ? parentBg : bg; }

} // namespace Ui::Res::Type
