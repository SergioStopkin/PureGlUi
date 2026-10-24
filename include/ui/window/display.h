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

#include <string>

namespace Ui::Window {

// One display the desktop runs on (NativeWindow::queryDisplays): its size in physical
// px and its refresh rate, 0 where it gives none
struct alignas(64) display_t final {
    std::string name; // the connector's or the screen's own
    int         width       = 0;
    int         height      = 0;
    fpx_t       refreshRate = 0.0F;
    bool        isPrimary   = false; // the one the app's frame pace is read from

    bool operator==(const display_t &) const = default;
};

} // namespace Ui::Window
