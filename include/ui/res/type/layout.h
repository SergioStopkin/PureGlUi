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

// Fields are grouped by member size, largest first, to minimize alignment
// padding; trailing comments carry each field's meaning.
struct alignas(128) layout_t final {
    // Regions (from layout.json element blocks)
    Ui::Res::Type::region_t topMenu;
    Ui::Res::Type::region_t topMenuDropdown; // popup dropdown styling (border-radius etc)
    Ui::Res::Type::region_t leftToolbar;
    Ui::Res::Type::region_t rightToolbar;
    Ui::Res::Type::region_t statusBar;
    Ui::Res::Type::region_t workspace;
    Ui::Res::Type::region_t workspaceTab;
    Ui::Res::Type::region_t dialog; // dialog window layout

    // Composite primitives
    Ui::Res::Dock::dock_layout_t
    dockDefaults; // shared dimensional/interaction tunables (grip width, min/max, snap, click threshold)
    theme_preview_t themePreview; // theme preview swatch geometry (rendered next to each theme submenu entry)

    // Borders + bounds
    Ui::Res::Type::border_t menuButtonHoverBorder;  // top-menu-button-label:hover border-radius
    Ui::Res::Type::border_t menuButtonActiveBorder; // top-menu-button-label:active border-radius
    Ui::Res::Type::border_t menuItemHoverBorder;    // top-menu-item:hover border-radius
    Ui::Res::Type::border_t tabCloseBorderRadius;   // workspace-tab-close border-radius
    Ui::Res::Type::bound_t  dialogIcon { 16, 16, 20, 20 };
    Ui::Res::Type::border_t dialogCloseBorder;
    Ui::Res::Type::border_t dialogButtonBorder;
    Ui::Res::Type::border_t dialogScrollbarBorder;
    Ui::Res::Type::border_t dialogScrollbarHoverBorder;

    // Containers + strings
    std::vector<Ui::Res::Dock::dock_config_t>
                docks;            // per-dock anchor + order + first-launch state (from "docks" array in layout.json)
    std::string tabCloseIcon;     // workspace-tab-close icon
    std::string tabArrowIconLeft; // workspace-tab-arrow icons
    std::string tabArrowIconRight;
    std::string dialogCloseIcon;

    // Scalars (from layout.json element blocks + :root vars)
    int   menuMaxDepth   = 3;          // --menu-max-depth: nesting cap, bar = 1 (3 = bar/dropdown/submenu)
    fpx_t menuButtonPadH = 16;         // top-menu-button-label horizontal padding
    fpx_t menuItemHoverMarginV {};     // top-menu-item:hover vertical margin (px)
    fpx_t menuItemHoverMarginH {};     // top-menu-item:hover horizontal margin (px)
    fpx_t menuItemIconWidth   = 16;    // top-menu-item-icon: width (svg render box)
    int   buttonImgSize       = 24;    // :root --button-img-size
    fpx_t windowWidth         = 1600;  // :root --window-width
    fpx_t windowHeight        = 1000;  // :root --window-height
    float iconHoverShadowX    = 2.0F;  // :root --button-icon-hover-shadow-x
    float iconHoverShadowY    = 2.0F;  // :root --button-icon-hover-shadow-y
    float iconHoverShadowBlur = 4.0F;  // :root --button-icon-hover-shadow-blur
    float iconActiveScale     = 0.85F; // :root --button-icon-active-scale
    fpx_t fallbackCharWidth   = 8;     // :root --fallback-char-width (text measurement fallback)
    fpx_t tabMinWidth {};              // workspace-tab min-width (shrinking)
    fpx_t tabCloseMargin {};           // workspace-tab-close style
    fpx_t tabCloseRight {};
    fpx_t tabCloseIconSize {};
    fpx_t tabArrowWidth {}; // workspace-tab-arrow dimensions
    fpx_t tabArrowHeight {};
    int   progressSectors   = 3;  // tabW / TL_radius (ensures each sector >= radius width)
    fpx_t dialogTitleHeight = 24; // dialog title bar
    fpx_t dialogTitleMargin {};   // gap below title before text
    fpx_t dialogTextMargin {};    // gap below text before buttons
    fpx_t dialogCloseSize   = 12; // dialog close button (X)
    fpx_t dialogCloseMargin = 2;
    fpx_t dialogCloseTop    = 8;
    fpx_t dialogCloseRight  = 8;
    fpx_t dialogButtonH {}; // dialog action buttons (OK, Cancel, etc.)
    fpx_t dialogButtonPad {};
    fpx_t dialogButtonMinW {};
    fpx_t dialogButtonShift {};
    fpx_t dialogScrollbarW {}; // dialog scrollbar
    fpx_t dialogScrollbarRight {};
    fpx_t dialogScrollbarMinThumb {};
    fpx_t dialogScrollbarHoverW {}; // dialog scrollbar hover
    fpx_t dialogScrollbarHoverRight {};
    fpx_t dialogScrollbarHoverMinThumb {};

    bool operator==(const layout_t &) const = default;
};

} // namespace Ui::Res::Type
