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

// CSS PROPERTY names read from a block in res/layout.json or a theme file - the
// single source of truth for every property spelling either store reads,
// resolved through cssPropKeyName(). A property may be read by only one store
// (layout reads geometry, the theme reads colors/fonts) but the spelling
// vocabulary is centralized so it is written once. Block selectors live in
// ElementKey; per-store :root custom-property variables live in LayoutKey /
// ThemeKey. Spellings stay in CSS kebab convention.
enum class CssPropKey : unsigned char {
    Color,           // color
    Background,      // background
    Width,           // width
    Height,          // height
    Margin,          // margin
    MarginBottom,    // margin-bottom
    Padding,         // padding
    BorderRadius,    // border-radius
    Top,             // top
    Left,            // left
    Right,           // right
    Bottom,          // bottom
    MinWidth,        // min-width
    MinHeight,       // min-height
    LineHeight,      // line-height
    ActiveContrast,  // active-contrast
    FontFamily,      // font-family
    FontSize,        // font-size
    FontWeight,      // font-weight
    Icon,            // icon
    IconLeft,        // icon-left
    IconRight,       // icon-right
    Shift,           // shift
    SplitAngle,      // split-angle
    MinThumbHeight,  // min-thumb-height
    GripWidth,       // grip-width
    GripIcon,        // grip-icon
    ClickThreshold,  // click-threshold
    RowIndent,       // row-indent
    RowExpanderSize, // row-expander-size
    RowKeyRatio,     // row-key-ratio

    Count, // enumerator total, never a property - what an exhaustive check counts against
};

[[nodiscard]] inline std::string cssPropKeyName(CssPropKey key)
{
    switch (key) {
    case CssPropKey::Color: return "color";
    case CssPropKey::Background: return "background";
    case CssPropKey::Width: return "width";
    case CssPropKey::Height: return "height";
    case CssPropKey::Margin: return "margin";
    case CssPropKey::MarginBottom: return "margin-bottom";
    case CssPropKey::Padding: return "padding";
    case CssPropKey::BorderRadius: return "border-radius";
    case CssPropKey::Top: return "top";
    case CssPropKey::Left: return "left";
    case CssPropKey::Right: return "right";
    case CssPropKey::Bottom: return "bottom";
    case CssPropKey::MinWidth: return "min-width";
    case CssPropKey::MinHeight: return "min-height";
    case CssPropKey::LineHeight: return "line-height";
    case CssPropKey::ActiveContrast: return "active-contrast";
    case CssPropKey::FontFamily: return "font-family";
    case CssPropKey::FontSize: return "font-size";
    case CssPropKey::FontWeight: return "font-weight";
    case CssPropKey::Icon: return "icon";
    case CssPropKey::IconLeft: return "icon-left";
    case CssPropKey::IconRight: return "icon-right";
    case CssPropKey::Shift: return "shift";
    case CssPropKey::SplitAngle: return "split-angle";
    case CssPropKey::MinThumbHeight: return "min-thumb-height";
    case CssPropKey::GripWidth: return "grip-width";
    case CssPropKey::GripIcon: return "grip-icon";
    case CssPropKey::ClickThreshold: return "click-threshold";
    case CssPropKey::RowIndent: return "row-indent";
    case CssPropKey::RowExpanderSize: return "row-expander-size";
    case CssPropKey::RowKeyRatio: return "row-key-ratio";

    case CssPropKey::Count: break;
    }
    return {};
}

} // namespace Ui::Res::Key
