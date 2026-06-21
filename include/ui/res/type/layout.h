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

#include "ui/res/dock/config.h"
#include "ui/res/dock/layout.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/region.h"
#include "ui/res/type/themepreview.h"

#include <string>
#include <vector>

namespace Ui::Res::Type {

struct alignas(128) layout_t final {
    Ui::Res::Type::region_t topMenu;
    Ui::Res::Type::region_t topMenuDropdown; // popup dropdown styling (border-radius etc)
    Ui::Res::Type::region_t leftToolbar;
    Ui::Res::Type::region_t rightToolbar;
    Ui::Res::Type::region_t statusBar;
    Ui::Res::Type::region_t workspace;
    Ui::Res::Type::region_t workspaceTab;

    // Parsed numeric values (from layout.json element-specific blocks)
    fpx_t                   menuButtonPadH = 16;     // top-menu-button-label horizontal padding
    Ui::Res::Type::border_t menuButtonHoverBorder;   // top-menu-button-label:hover border-radius
    Ui::Res::Type::border_t menuButtonActiveBorder;  // top-menu-button-label:active border-radius
    Ui::Res::Type::border_t menuItemHoverBorder;     // top-menu-item:hover border-radius
    fpx_t                   menuItemHoverMarginV {}; // top-menu-item:hover vertical margin (px)
    fpx_t                   menuItemHoverMarginH {}; // top-menu-item:hover horizontal margin (px)
    fpx_t                   menuItemIconWidth = 16;  // top-menu-item-icon: width (svg render box)
    int                     buttonImgSize     = 24;  // :root --button-img-size

    // Window defaults (from :root)
    fpx_t windowWidth  = 1600; // :root --window-width
    fpx_t windowHeight = 1000; // :root --window-height

    // Button icon interaction params (from :root)
    float iconHoverShadowX    = 2.0F;  // :root --button-icon-hover-shadow-x
    float iconHoverShadowY    = 2.0F;  // :root --button-icon-hover-shadow-y
    float iconHoverShadowBlur = 4.0F;  // :root --button-icon-hover-shadow-blur
    float iconActiveScale     = 0.85F; // :root --button-icon-active-scale

    // Fallback character width for text measurement when font renderer unavailable
    fpx_t fallbackCharWidth = 8; // :root --fallback-char-width

    // Workspace tab min width for shrinking (from layout.json "workspace-tab" min-width)
    fpx_t tabMinWidth {};

    // Workspace tab close button style (from layout.json "workspace-tab-close")
    fpx_t                   tabCloseMargin {};
    fpx_t                   tabCloseRight {};
    fpx_t                   tabCloseIconSize {};
    Ui::Res::Type::border_t tabCloseBorderRadius;
    std::string             tabCloseIcon;

    // Workspace tab arrow dimensions and icons (from layout.json "workspace-tab-arrow")
    fpx_t       tabArrowWidth {};
    fpx_t       tabArrowHeight {};
    std::string tabArrowIconLeft;
    std::string tabArrowIconRight;

    // Progress bar sectors: tabW / TL_radius (ensures each sector >= radius width)
    int progressSectors = 3;

    // Theme preview swatch geometry (rendered next to each theme submenu entry)
    theme_preview_t themePreview;

    // Dialog window layout
    Ui::Res::Type::region_t dialog;
    fpx_t                   dialogTitleHeight = 24;
    fpx_t                   dialogTitleMargin {}; // gap below title before text
    Ui::Res::Type::bound_t  dialogIcon { 16, 16, 20, 20 };
    fpx_t                   dialogTextMargin {}; // gap below text before buttons

    // Dialog close button (X)
    fpx_t                   dialogCloseSize = 12;
    Ui::Res::Type::border_t dialogCloseBorder;
    fpx_t                   dialogCloseMargin = 2;
    fpx_t                   dialogCloseTop    = 8;
    fpx_t                   dialogCloseRight  = 8;
    std::string             dialogCloseIcon;

    // Dialog action buttons (OK, Cancel, etc.)
    fpx_t                   dialogButtonH {};
    Ui::Res::Type::border_t dialogButtonBorder;
    fpx_t                   dialogButtonPad {};
    fpx_t                   dialogButtonMinW {};
    fpx_t                   dialogButtonShift {};

    // Dialog scrollbar
    fpx_t                   dialogScrollbarW {};
    fpx_t                   dialogScrollbarRight {};
    Ui::Res::Type::border_t dialogScrollbarBorder;
    fpx_t                   dialogScrollbarMinThumb {};

    // Dialog scrollbar hover
    fpx_t                   dialogScrollbarHoverW {};
    fpx_t                   dialogScrollbarHoverRight {};
    Ui::Res::Type::border_t dialogScrollbarHoverBorder;
    fpx_t                   dialogScrollbarHoverMinThumb {};

    // Dock primitive
    Ui::Res::Dock::dock_layout_t
    dockDefaults; // shared dimensional/interaction tunables (grip width, min/max, snap, click threshold)
    std::vector<Ui::Res::Dock::dock_config_t>
    docks; // per-dock anchor + order + first-launch state (from "docks" array in layout.json)

    bool operator==(const layout_t &) const = default;
};

} // namespace Ui::Res::Type
