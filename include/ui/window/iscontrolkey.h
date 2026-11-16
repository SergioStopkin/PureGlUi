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

#include <string_view>

namespace Ui::Window {

// The two Ctrl keys' names, X11's, which every other platform's events name them after
inline constexpr std::string_view CONTROL_LEFT  = "Control_L";
inline constexpr std::string_view CONTROL_RIGHT = "Control_R";

// Whether a key event's name is a Ctrl key - either one, and on macOS Cmd too, which
// the framework folds into Control
[[nodiscard]] inline bool isControlKey(std::string_view name) { return name == CONTROL_LEFT || name == CONTROL_RIGHT; }

} // namespace Ui::Window
