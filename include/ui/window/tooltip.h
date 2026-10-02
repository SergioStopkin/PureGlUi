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

#include "ui/res/type/bound.h"

#include <string>

namespace Ui::Window {

// What a tooltip says and what it stands beside, in main-window CSS px; an empty
// text is none
struct alignas(64) tooltip_t final {
    std::string            text;
    Ui::Res::Type::bound_t anchor;

    bool operator==(const tooltip_t &) const = default;
};

} // namespace Ui::Window
