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

// Pointer shapes the framework can ask for. Deliberately short: each entry
// costs a name mapping on four platforms, so one is added when something
// actually needs it, not in anticipation.
enum class PointerShape : unsigned char {
    Default,   // the normal arrow
    Crosshair, // precise picking - measurement, point entry
    ResizeH,   // horizontal resize - dock grip drag
    Move,      // grabbing and dragging content - viewport pan
};

// Cursor-theme name, as wl_cursor_theme_get_cursor wants it. X11 goes through
// XCreateFontCursor with XC_* constants instead, so it maps separately rather
// than pulling in libXcursor just to share these strings.
[[nodiscard]] inline const char * pointerShapeToThemeName(PointerShape shape)
{
    switch (shape) {
    case PointerShape::Crosshair: return "crosshair";
    case PointerShape::ResizeH: return "sb_h_double_arrow";
    case PointerShape::Move: return "fleur";
    case PointerShape::Default: return "left_ptr";
    }
    return "left_ptr";
}

} // namespace Ui::Window
