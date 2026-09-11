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

// Values match X11 button numbers, which is what toMouseButton() converts
enum class MouseButton : unsigned char { Left = 1, Middle = 2, Right = 3 };

inline int toInt(MouseButton button) { return static_cast<int>(button); }

inline MouseButton toMouseButton(int value) { return static_cast<MouseButton>(value); }

} // namespace Ui::Window
