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

#include <cstdint>

namespace Ui::Res::Type {

enum class Changed : uint16_t {
    None     = 0,
    Locale   = 0x0001,
    Theme    = 0x0002,
    Layout   = 0x0004,
    Popup    = 0x0008,
    Shortcut = 0x0010,
    Menu     = 0x0020,
    Button   = 0x0040,
    Render   = 0x0080,
    Icon     = 0x0100,
    All      = 0xFFFF
};

} // namespace Ui::Res::Type
