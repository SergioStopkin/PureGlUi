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

namespace Ui {

// Flip to true to trace the chrome orchestration: every click and key as it is
// classified, menu hover, and the popup lifecycle with its geometry. One line
// or more per input event, so this is loud under any real use.
constexpr bool SHELL_DEBUG = false;

// Flip to true to trace how the pointer switches the open list across the menu bar: a
// move onto a button (from the main window or from over the list), what MenuBar::hover
// decides and why, a held hover and its replay, and the waits that hold a switch off
constexpr bool MENU_HOVER_DEBUG = false;

} // namespace Ui
