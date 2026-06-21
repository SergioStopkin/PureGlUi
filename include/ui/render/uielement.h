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

namespace Ui::Render {

// Types of UI elements (used by renderers, windows, and app)
enum class UiElementType : unsigned char {
    MenuButton,    // Top menu bar button ("File", "Edit", etc.)
    ToolbarButton, // Left/right toolbar icon button
    MenuItem,      // Popup dropdown menu item row
    Separator,     // Popup dropdown separator line
    Text,          // Text-only element (status bar text, labels)
    Image,         // SVG icon image
    Tab,           // Workspace tab
    TabClose,      // Workspace tab close button (X)
    TabArrow       // Workspace tab scroll arrow ('<' or '>')
};

inline int toInt(UiElementType type) { return static_cast<int>(type); }

} // namespace Ui::Render
