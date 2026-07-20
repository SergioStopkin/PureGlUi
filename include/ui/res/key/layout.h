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

#include <string>

namespace Ui::Res::Key {

// The res/layout.json :root CSS custom-property variables the LayoutStore looks
// up directly (the var(--X) name is the map key) - the single source of truth
// for their spelling, resolved through layoutKeyName(). Block selectors live in
// ElementKey and property names in CssPropKey (both shared with the theme);
// these variables are layout-only, so they live here. Spellings keep the CSS
// custom-property (--kebab) convention.
enum class LayoutKey : unsigned char {
    ButtonImgSize,             // --button-img-size
    WindowWidth,               // --window-width
    WindowHeight,              // --window-height
    ButtonIconHoverShadowX,    // --button-icon-hover-shadow-x
    ButtonIconHoverShadowY,    // --button-icon-hover-shadow-y
    ButtonIconHoverShadowBlur, // --button-icon-hover-shadow-blur
    ButtonIconActiveScale,     // --button-icon-active-scale
    FallbackCharWidth,         // --fallback-char-width
    MenuMaxDepth,              // --menu-max-depth
};

[[nodiscard]] inline std::string layoutKeyName(LayoutKey key)
{
    switch (key) {
    case LayoutKey::ButtonImgSize: return "--button-img-size";
    case LayoutKey::WindowWidth: return "--window-width";
    case LayoutKey::WindowHeight: return "--window-height";
    case LayoutKey::ButtonIconHoverShadowX: return "--button-icon-hover-shadow-x";
    case LayoutKey::ButtonIconHoverShadowY: return "--button-icon-hover-shadow-y";
    case LayoutKey::ButtonIconHoverShadowBlur: return "--button-icon-hover-shadow-blur";
    case LayoutKey::ButtonIconActiveScale: return "--button-icon-active-scale";
    case LayoutKey::FallbackCharWidth: return "--fallback-char-width";
    case LayoutKey::MenuMaxDepth: return "--menu-max-depth";
    }
    return {};
}

} // namespace Ui::Res::Key
