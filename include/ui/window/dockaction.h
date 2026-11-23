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

namespace Ui::Window {

// What the window manager does after a dock gesture (DockDrag)
enum class DockAction : unsigned char {
    Pass,    // no dock gesture, or its dock went away: the event goes on as usual
    Consume, // the gesture took the event, nothing more to do
    Refresh, // a dock's rows moved: their hit rects built again (requestContentRefresh)
    Reflow,  // a dock's width changed: margins, docks and content surfaces laid out again
};

} // namespace Ui::Window
