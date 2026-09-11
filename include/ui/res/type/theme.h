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

#include "ui/res/dock/theme.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/font.h"
#include "ui/type.h"

namespace Ui::Res::Type {

struct alignas(128) theme_t final {
    // Per-element font properties (family, size, weight from theme JSON blocks)
    Ui::Res::Type::font_t topMenuFont;      // "top-menu"               (fallback: fontSans, 16, regular)
    Ui::Res::Type::font_t menuItemFont;     // "top-menu-item"           (fallback: fontSans, 16, regular)
    Ui::Res::Type::font_t shortcutFont;     // "top-menu-item-shortcut"  (fallback: fontMono, 16, regular)
    Ui::Res::Type::font_t leftToolbarFont;  // "left-toolbar"            (fallback: fontSans, 24, bold)
    Ui::Res::Type::font_t buttonFont;       // "button"                  (fallback: fontSans, 24, bold)
    Ui::Res::Type::font_t rightToolbarFont; // "right-toolbar"           (fallback: fontSans, 24, regular)
    Ui::Res::Type::font_t statusBarFont;    // "status-bar"              (fallback: fontMono, 14, regular)
    Ui::Res::Type::font_t workspaceTabFont; // "workspace-tab"           (fallback: fontSans, 14, regular)

    // Root CSS variables (from :root block)
    Ui::Res::Type::color_pair_t main;   // --cl-main + --bg-main
    Ui::Res::Type::color_pair_t second; // --cl-second + --bg-second

    // Regions (Ui::Res::Type::color_pair_t {fg, bg} -- no hover/active)
    Ui::Res::Type::color_pair_t topMenu;       // "top-menu"
    Ui::Res::Type::color_pair_t dropdown;      // "top-menu-dropdown"
    Ui::Res::Type::color_pair_t leftToolbar;   // "left-toolbar"
    Ui::Res::Type::color_pair_t rightToolbar;  // "right-toolbar"
    Ui::Res::Type::color_pair_t workspace;     // "workspace"
    Ui::Res::Type::color_pair_t workspaceTabs; // "workspace-tabs" (tab container strip)

    // Interactive elements: normal {fg, bg} + hover {fg, bg} + active {fg, bg}
    Ui::Res::Type::color_pair_t topMenuButton;       // inherits topMenu {fg, bg}
    Ui::Res::Type::color_pair_t topMenuButtonHover;  // "top-menu-button-label:hover"
    Ui::Res::Type::color_pair_t topMenuButtonActive; // "top-menu-button-label:active"
    Ui::Res::Type::color_pair_t menuItem;            // "top-menu-item" color + transparent bg
    Ui::Res::Type::color_pair_t menuItemHover;       // "top-menu-item:hover"
    Ui::Res::Type::color_pair_t menuItemActive;      // "top-menu-item:active"
    Ui::Res::Type::color_pair_t button;              // "button" color + background
    // HSL lightness delta marking an armed toolbar button, in percentage
    // points. Applied through Color::edgeColor so it lightens on a dark theme
    // and darkens on a light one, rather than needing a second colour block
    fpx_t                       buttonActiveContrast = 12.0F; // "button" active-contrast
    Ui::Res::Type::color_pair_t workspaceTab;                 // "workspace-tab" color + background
    Ui::Res::Type::color_pair_t workspaceTabHover;            // "workspace-tab:hover"
    Ui::Res::Type::color_pair_t workspaceTabActive;           // "workspace-tab:active"
    Ui::Res::Type::color_pair_t tabClose;                     // "workspace-tab-close" color (tint)
    Ui::Res::Type::color_pair_t tabCloseHover;                // "workspace-tab-close:hover" color (tint)
    Ui::Res::Type::font_t       dialogFont;                   // "dialog" font
    fpx_t                       dialogLineHeight = 1.4F;      // "dialog" line-height
    Ui::Res::Type::color_pair_t dialog;                       // "dialog" color + background
    Ui::Res::Type::font_t       dialogTitleFont;              // "dialog-title" font
    Ui::Color                   dialogTitleColor;             // "dialog-title" color
    Ui::Color                   dialogLinkColor;              // "dialog-link" color
    Ui::Color                   dialogLinkVisited;            // "dialog-link:visited" color
    Ui::Res::Type::color_pair_t dialogButton;                 // "dialog-button" color + background
    Ui::Res::Type::color_pair_t dialogButtonHover;            // "dialog-button:hover" color + background
    Ui::Res::Type::color_pair_t dialogButtonActive;           // "dialog-button:active" color + background
    Ui::Res::Type::color_pair_t dialogButtonPrimary;          // "dialog-button:primary" color + background
    Ui::Color                   scrollbarTrack;               // "scrollbar" background
    Ui::Color                   scrollbarThumb;               // "scrollbar-thumb" background
    Ui::Color                   scrollbarThumbHover;          // "scrollbar-thumb:hover" background
    Ui::Res::Type::color_pair_t dialogClose;                  // "dialog-close" color (tint)
    Ui::Res::Type::color_pair_t dialogCloseHover;             // "dialog-close:hover" color (tint) + background
    Ui::Res::Type::color_pair_t dialogCloseActive;            // "dialog-close:active" color (tint) + background
    Ui::Res::Type::color_pair_t tabArrow;                     // "workspace-tab-arrow" color (tint) + transparent bg
    Ui::Res::Type::color_pair_t statusBar;                    // "status-bar"
    Ui::Res::Type::color_pair_t statusBarActive;              // "status-bar:active"

    Ui::Res::Type::FontWeight workspaceTabActiveWeight =
    Ui::Res::Type::FontWeight::Bold; // "workspace-tab:active" font-weight

    // Standalone colors (no pairing needed)
    Ui::Color colorShadow;
    fpx_t     shadowOpacity {};
    Ui::Color colorModel;
    Ui::Color colorError;
    Ui::Color colorLoad;
    Ui::Color colorInfo;             // "--cl-info" (Info dialog icon tint)
    Ui::Color colorWarn;             // "--cl-warn" (Warning dialog icon tint)
    Ui::Color menuItemDisabledColor; // "top-menu-item:disabled" color
    Ui::Color separatorColor;        // "top-menu-separator"     background
    Ui::Color shortcutColor;         // "top-menu-item-shortcut" color
    Ui::Color shortcutHoverColor;    // "top-menu-item-shortcut:hover" color

    // Dock primitive (one shared theme for all dock instances; left/right + inner/outer same style)
    Ui::Res::Dock::dock_theme_t dock;

    bool operator==(const theme_t &) const = default;
};

} // namespace Ui::Res::Type
