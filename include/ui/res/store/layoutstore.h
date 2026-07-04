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

#include "common/bit.h"
#include "common/json.h"
#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/config.h"
#include "ui/convert.h"
#include "ui/res/dock/config.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/layout.h"
#include "ui/res/type/popup.h"
#include "ui/res/type/region.h"

#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Res::Store {

// Loads and holds res/css/layout.json: the layout_t geometry block plus the
// derived popup_t metrics. One of the logical sub-stores ResManager composes.
// load() parses the file once and returns the combined Changed::Layout |
// Changed::Popup bits for whatever actually changed, so the facade can aggregate.
class LayoutStore final {
    Ui::Res::Type::layout_t m_layout;
    Ui::Res::Type::popup_t  m_popup;

public:
    [[nodiscard]] const Ui::Res::Type::layout_t & layout() const { return m_layout; }
    [[nodiscard]] const Ui::Res::Type::popup_t &  popup() const { return m_popup; }

    // Dock definitions are part of layout_t but loaded by a separate host pass
    // (ResManager::loadDocks). Commit them here so m_layout stays encapsulated;
    // returns Changed::Layout when the set actually changed.
    [[nodiscard]] Ui::Res::Type::Changed setDocks(std::vector<Ui::Res::Dock::dock_config_t> docks)
    {
        if (m_layout.docks == docks) {
            return Ui::Res::Type::Changed::None;
        }
        m_layout.docks = std::move(docks);
        return Ui::Res::Type::Changed::Layout;
    }

    // Derive popup metrics from the (already-parsed) layout JSON. layout.json
    // carries the top-menu-item / top-menu-separator styling popup_t needs, so
    // load() computes both layout_t and popup_t from one parse. `j == nullptr`
    // means the file failed to load -> apply CSS defaults. Returns Changed::Popup
    // when the metrics actually changed.
    Ui::Res::Type::Changed loadPopupFrom(const nlohmann::json * j)
    {
        const Ui::Res::Type::popup_t oldPopup = m_popup;

        // CSS string defaults
        std::string fontSize        = "14px";
        std::string padding         = "6px 16px";
        std::string lineHeight      = "1.38";
        std::string separatorHeight = "1px";
        std::string separatorMargin = "4px 0";

        if (j != nullptr) {
            // Load top-menu-item styling
            if (j->contains("top-menu-item") && (*j)["top-menu-item"].is_object()) {
                const auto & item = (*j)["top-menu-item"];
                fontSize          = item.value("font-size", fontSize);
                padding           = item.value("padding", padding);
                lineHeight        = item.value("line-height", lineHeight);
            }
            // Load separator styling
            if (j->contains("top-menu-separator") && (*j)["top-menu-separator"].is_object()) {
                const auto & sep = (*j)["top-menu-separator"];
                separatorHeight  = sep.value("height", separatorHeight);
                separatorMargin  = sep.value("margin", separatorMargin);
            }
        }

        calculatePopupHeights(fontSize, lineHeight, padding, separatorHeight, separatorMargin);

        return m_popup == oldPopup ? Ui::Res::Type::Changed::None : Ui::Res::Type::Changed::Popup;
    }

    void calculatePopupHeights(const std::string & fontSize,
                               const std::string & lineHeight,
                               const std::string & padding,
                               const std::string & separatorHeight,
                               const std::string & separatorMargin)
    {
        // Parse item padding "Vpx Hpx"
        m_popup.itemPaddingV = Ui::Convert::parseCssNumber(padding);
        auto sp              = padding.find(' ');
        if (sp != std::string::npos) {
            m_popup.itemPaddingH = Ui::Convert::parseCssNumber(padding.substr(sp + 1));
        } else {
            m_popup.itemPaddingH = m_popup.itemPaddingV;
        }

        // Item height = fontSize * lineHeight + paddingTop + paddingBottom
        m_popup.itemHeight = Ui::Convert::parseCssNumber(fontSize) * Ui::Convert::parseCssNumber(lineHeight)
                           + m_popup.itemPaddingV * 2.0F;

        // Parse separator height and margin "Vpx Hpx"
        m_popup.separatorHeight  = Ui::Convert::parseCssNumber(separatorHeight);
        m_popup.separatorMarginV = Ui::Convert::parseCssNumber(separatorMargin);
        sp                       = separatorMargin.find(' ');
        if (sp != std::string::npos) {
            m_popup.separatorMarginH = Ui::Convert::parseCssNumber(separatorMargin.substr(sp + 1));
        } else {
            m_popup.separatorMarginH = m_popup.separatorMarginV;
        }
    }

    // Load res/css/layout.json once, deriving BOTH layout_t and popup_t from the
    // single parse. Returns the combined Changed bits. On file-load failure the
    // popup falls back to CSS defaults (layout is left untouched).
    Ui::Res::Type::Changed load(const std::string & file)
    {
        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return loadPopupFrom(nullptr);
        }

        const Ui::Res::Type::layout_t oldLayout = m_layout;

        // Parse :root block for CSS variables
        std::unordered_map<std::string, std::string> cssVariables;
        if (j.contains(":root") && j[":root"].is_object()) {
            for (auto it = j[":root"].begin(); it != j[":root"].end(); ++it) {
                if (it.value().is_string()) {
                    cssVariables[it.key()] = it.value().get<std::string>();
                }
            }
        }

        std::cout << "[ KEY ]:[ VALUE ]" << std::endl;
        for (const auto & [key, value] : cssVariables) {
            std::cout << "[" << key << "]:[" << value << "]" << std::endl;
        }

        // Helper to resolve var(--...) in a value
        auto resolveVariable = [&cssVariables](const std::string & value) -> std::string {
            const std::string prefix  = "var(";
            const std::string postfix = ")";

            auto start = value.find(prefix);
            if (start != std::string::npos) {
                auto end = value.find(postfix, start);
                if (end != std::string::npos) {
                    const auto variable = value.substr(start + prefix.size(), end - start - prefix.size());

                    if (cssVariables.find(variable) != cssVariables.end()) {
                        return cssVariables[variable];
                    }
                }
            }

            return value;
        };

        auto parseRegion = [&resolveVariable](const nlohmann::json & regionJson) -> Ui::Res::Type::region_t {
            Ui::Res::Type::region_t region;
            region.height  = Ui::Convert::str2fpx(resolveVariable(regionJson.value("height", "")));
            region.width   = Ui::Convert::str2fpx(resolveVariable(regionJson.value("width", "")));
            region.margin  = Ui::Convert::str2fpx(resolveVariable(regionJson.value("margin", "")));
            region.padding = Ui::Convert::str2fpx(resolveVariable(regionJson.value("padding", "")));
            region.top     = Ui::Convert::str2fpx(resolveVariable(regionJson.value("top", "")));
            region.left    = Ui::Convert::str2fpx(resolveVariable(regionJson.value("left", "")));
            region.right   = Ui::Convert::str2fpx(resolveVariable(regionJson.value("right", "")));
            region.bottom  = Ui::Convert::str2fpx(resolveVariable(regionJson.value("bottom", "")));
            // Read CSS border-radius and parse into numeric border values
            const std::string brs = resolveVariable(regionJson.value("border-radius", ""));
            if (!brs.empty()) {
                region.border = Ui::Convert::parseCssBorderRadius(brs);
            }

            return region;
        };

        if (j.contains("top-menu") && j["top-menu"].is_object()) {
            m_layout.topMenu = parseRegion(j["top-menu"]);
        }
        if (j.contains("left-toolbar") && j["left-toolbar"].is_object()) {
            m_layout.leftToolbar = parseRegion(j["left-toolbar"]);
        }
        if (j.contains("right-toolbar") && j["right-toolbar"].is_object()) {
            m_layout.rightToolbar = parseRegion(j["right-toolbar"]);
        }
        if (j.contains("status-bar") && j["status-bar"].is_object()) {
            m_layout.statusBar = parseRegion(j["status-bar"]);
        }
        if (j.contains("workspace") && j["workspace"].is_object()) {
            m_layout.workspace = parseRegion(j["workspace"]);
        }
        if (j.contains("workspace-tab") && j["workspace-tab"].is_object()) {
            const auto & wsTab    = j["workspace-tab"];
            m_layout.workspaceTab = parseRegion(wsTab);
            m_layout.tabMinWidth  = Ui::Convert::str2fpx(resolveVariable(wsTab.value("min-width", "")));
            // Compute progress sectors: tabW / TL_radius
            const int tlr = Ui::roundToInt(m_layout.workspaceTab.border.topLeft);
            if (tlr > 0) {
                m_layout.progressSectors = static_cast<int>(m_layout.workspaceTab.width) / tlr;
                if (m_layout.progressSectors < 2) {
                    m_layout.progressSectors = 2;
                }
            }
        }

        // Parse workspace-tab-close layout (icon filename + dimensions)
        if (j.contains("workspace-tab-close") && j["workspace-tab-close"].is_object()) {
            const auto & close            = j["workspace-tab-close"];
            m_layout.tabCloseMargin       = Ui::Convert::str2fpx(resolveVariable(close.value("margin", "")));
            m_layout.tabCloseRight        = Ui::Convert::str2fpx(resolveVariable(close.value("right", "")));
            m_layout.tabCloseIconSize     = Ui::Convert::str2fpx(resolveVariable(close.value("height", "")));
            m_layout.tabCloseBorderRadius = Ui::Convert::parseCssBorderRadius(
            resolveVariable(close.value("border-radius", "")));
            m_layout.tabCloseIcon = resolveVariable(close.value("icon", "var(--close-icon)"));
        }

        // Parse workspace-tab-arrow layout (icon filenames + dimensions)
        if (j.contains("workspace-tab-arrow") && j["workspace-tab-arrow"].is_object()) {
            const auto & arrow         = j["workspace-tab-arrow"];
            m_layout.tabArrowWidth     = Ui::Convert::str2fpx(resolveVariable(arrow.value("width", "")));
            m_layout.tabArrowHeight    = Ui::Convert::str2fpx(resolveVariable(arrow.value("height", "")));
            m_layout.tabArrowIconLeft  = arrow.value("icon-left", m_layout.tabArrowIconLeft);
            m_layout.tabArrowIconRight = arrow.value("icon-right", m_layout.tabArrowIconRight);
        }

        // Parse top-menu-dropdown region used for popup styling (border-radius etc.)
        if (j.contains("top-menu-dropdown") && j["top-menu-dropdown"].is_object()) {
            m_layout.topMenuDropdown = parseRegion(j["top-menu-dropdown"]);
        }

        // Parse theme-preview swatch (rendered next to each theme name in submenu).
        // Layout only; per-theme colors are populated by scanThemeNames.
        if (j.contains("theme-preview") && j["theme-preview"].is_object()) {
            const auto & tp              = j["theme-preview"];
            m_layout.themePreview.width  = Ui::Convert::str2fpx(resolveVariable(tp.value("width", "")));
            m_layout.themePreview.height = Ui::Convert::str2fpx(resolveVariable(tp.value("height", "")));
            m_layout.themePreview.border = Ui::Convert::parseCssBorderRadius(
            resolveVariable(tp.value("border-radius", "")));
            m_layout.themePreview.right      = Ui::Convert::str2fpx(resolveVariable(tp.value("right", "")));
            m_layout.themePreview.splitAngle = Ui::Convert::parseCssNumber(
            resolveVariable(tp.value("split-angle", "45")));
        }

        // Parse element-specific layout values
        if (j.contains("top-menu-button-label") && j["top-menu-button-label"].is_object()) {
            const auto &      btn = j["top-menu-button-label"];
            const std::string pad = btn.value("padding", "0 16px");
            auto              sp  = pad.find(' ');
            if (sp != std::string::npos) {
                m_layout.menuButtonPadH = Ui::Convert::str2fpx(pad.substr(sp + 1));
            }
        }
        if (j.contains("top-menu-button-label:hover") && j["top-menu-button-label:hover"].is_object()) {
            const std::string brs = resolveVariable(j["top-menu-button-label:hover"].value("border-radius", ""));
            if (!brs.empty()) {
                m_layout.menuButtonHoverBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
        }
        if (j.contains("top-menu-button-label:active") && j["top-menu-button-label:active"].is_object()) {
            const std::string brs = resolveVariable(j["top-menu-button-label:active"].value("border-radius", ""));
            if (!brs.empty()) {
                m_layout.menuButtonActiveBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
        }
        if (j.contains("top-menu-item:hover") && j["top-menu-item:hover"].is_object()) {
            const auto &      itemHover = j["top-menu-item:hover"];
            const std::string brs       = resolveVariable(itemHover.value("border-radius", ""));
            if (!brs.empty()) {
                m_layout.menuItemHoverBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
            const std::string margin = itemHover.value("margin", "");
            if (!margin.empty()) {
                std::istringstream ms(margin);
                std::string        tok;
                std::vector<fpx_t> vals;
                while (ms >> tok) {
                    vals.emplace_back(Ui::Convert::parseCssNumber(tok));
                }
                if (vals.size() == 1) {
                    m_layout.menuItemHoverMarginV = vals[0];
                    m_layout.menuItemHoverMarginH = vals[0];
                } else if (vals.size() >= 2) {
                    m_layout.menuItemHoverMarginV = vals[0];
                    m_layout.menuItemHoverMarginH = vals[1];
                }
            }
        }
        if (j.contains("top-menu-item-icon") && j["top-menu-item-icon"].is_object()) {
            const auto &      itemIcon = j["top-menu-item-icon"];
            const std::string width    = resolveVariable(itemIcon.value("width", ""));
            if (!width.empty()) {
                m_layout.menuItemIconWidth = Ui::Convert::parseCssNumber(width);
            }
        }
        if (cssVariables.contains("--button-img-size")) {
            m_layout.buttonImgSize = Ui::Convert::str2int(cssVariables["--button-img-size"]);
        }
        if (cssVariables.contains("--window-width")) {
            m_layout.windowWidth = Ui::Convert::str2fpx(cssVariables["--window-width"]);
        }
        if (cssVariables.contains("--window-height")) {
            m_layout.windowHeight = Ui::Convert::str2fpx(cssVariables["--window-height"]);
        }
        if (cssVariables.contains("--button-icon-hover-shadow-x")) {
            m_layout.iconHoverShadowX = Ui::Convert::parseCssNumber(cssVariables["--button-icon-hover-shadow-x"]);
        }
        if (cssVariables.contains("--button-icon-hover-shadow-y")) {
            m_layout.iconHoverShadowY = Ui::Convert::parseCssNumber(cssVariables["--button-icon-hover-shadow-y"]);
        }
        if (cssVariables.contains("--button-icon-hover-shadow-blur")) {
            m_layout.iconHoverShadowBlur = Ui::Convert::parseCssNumber(cssVariables["--button-icon-hover-shadow-blur"]);
        }
        if (cssVariables.contains("--button-icon-active-scale")) {
            m_layout.iconActiveScale = Ui::Convert::parseCssNumber(cssVariables["--button-icon-active-scale"]);
        }
        if (cssVariables.contains("--fallback-char-width")) {
            m_layout.fallbackCharWidth = Ui::Convert::str2fpx(cssVariables["--fallback-char-width"]);
        }
        if (cssVariables.contains("--menu-max-depth")) {
            // Nonpositive/malformed value keeps the default: a cap of 0 would
            // drop every submenu at the first descent.
            const int menuMaxDepth = Ui::Convert::str2int(cssVariables["--menu-max-depth"]);
            if (menuMaxDepth > 0) {
                m_layout.menuMaxDepth = menuMaxDepth;
            }
        }

        // Parse dialog layout
        if (j.contains("dialog") && j["dialog"].is_object()) {
            m_layout.dialog = parseRegion(j["dialog"]);
        }

        if (j.contains("dialog-title") && j["dialog-title"].is_object()) {
            const auto & dt            = j["dialog-title"];
            m_layout.dialogTitleHeight = Ui::Convert::parseCssNumber(resolveVariable(dt.value("height", "24px")));
            m_layout.dialogTitleMargin = Ui::Convert::parseCssNumber(resolveVariable(dt.value("margin-bottom", "")));
        }

        if (j.contains("dialog-text") && j["dialog-text"].is_object()) {
            const auto & dtxt         = j["dialog-text"];
            m_layout.dialogTextMargin = Ui::Convert::parseCssNumber(resolveVariable(dtxt.value("margin-bottom", "")));
        }

        if (j.contains("dialog-icon") && j["dialog-icon"].is_object()) {
            const auto & di     = j["dialog-icon"];
            m_layout.dialogIcon = { Ui::Convert::parseCssNumber(resolveVariable(di.value("left", "16px"))),
                                    Ui::Convert::parseCssNumber(resolveVariable(di.value("top", "16px"))),
                                    Ui::Convert::parseCssNumber(resolveVariable(di.value("width", "20px"))),
                                    Ui::Convert::parseCssNumber(resolveVariable(di.value("height", "20px"))) };
        }

        if (j.contains("dialog-close") && j["dialog-close"].is_object()) {
            const auto & dialogClose = j["dialog-close"];
            m_layout.dialogCloseSize = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value("height", "12px")));
            m_layout.dialogCloseBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogClose.value("border-radius", "")));
            m_layout.dialogCloseMargin = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value("margin", "2px")));
            m_layout.dialogCloseTop   = Ui::Convert::parseCssNumber(resolveVariable(dialogClose.value("top", "8px")));
            m_layout.dialogCloseRight = Ui::Convert::parseCssNumber(resolveVariable(dialogClose.value("right", "8px")));
            m_layout.dialogCloseIcon  = resolveVariable(dialogClose.value("icon", "var(--close-icon)"));
        }

        if (j.contains("dialog-button") && j["dialog-button"].is_object()) {
            const auto & dialogButton = j["dialog-button"];
            m_layout.dialogButtonH = Ui::Convert::parseCssNumber(resolveVariable(dialogButton.value("height", "28px")));
            m_layout.dialogButtonBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogButton.value("border-radius", "")));
            m_layout.dialogButtonPad = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value("padding", "8px")));
            m_layout.dialogButtonMinW = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value("min-width", "70px")));
            m_layout.dialogButtonShift = Ui::Convert::parseCssNumber(resolveVariable(dialogButton.value("shift", "")));
        }

        if (j.contains("dialog-scrollbar") && j["dialog-scrollbar"].is_object()) {
            const auto & dialogScrollbar = j["dialog-scrollbar"];
            m_layout.dialogScrollbarW    = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value("width", "")));
            m_layout.dialogScrollbarRight = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value("right", "")));
            m_layout.dialogScrollbarBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogScrollbar.value("border-radius", "")));
            m_layout.dialogScrollbarMinThumb = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value("min-thumb-height", "")));
        }

        if (j.contains("dialog-scrollbar:hover") && j["dialog-scrollbar:hover"].is_object()) {
            const auto & scrollbarHover    = j["dialog-scrollbar:hover"];
            m_layout.dialogScrollbarHoverW = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value("width", "")));
            m_layout.dialogScrollbarHoverRight = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value("right", "")));
            m_layout.dialogScrollbarHoverBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(scrollbarHover.value("border-radius", "")));
            m_layout.dialogScrollbarHoverMinThumb = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value("min-thumb-height", "")));
        }

        // Shared dock dimensional tunables. One block applies to every dock
        // instance; per-dock state (current width, collapsed flag) lives in
        // session, not layout.json.
        if (j.contains("dock-defaults") && j["dock-defaults"].is_object()) {
            const auto & dd                      = j["dock-defaults"];
            m_layout.dockDefaults.gripWidth      = Ui::Convert::str2fpx(resolveVariable(dd.value("grip-width", "")));
            m_layout.dockDefaults.gripRadius     = Ui::Convert::str2fpx(resolveVariable(dd.value("border-radius", "")));
            m_layout.dockDefaults.clickThreshold = Ui::Convert::str2fpx(
            resolveVariable(dd.value("click-threshold", "")));
            m_layout.dockDefaults.gripIcon = Common::Sanitize::filePath(resolveVariable(dd.value("grip-icon", "")),
                                                                        "dock.gripIcon");
        }

        const Ui::Res::Type::Changed layoutChanged = (m_layout == oldLayout) ? Ui::Res::Type::Changed::None
                                                                             : Ui::Res::Type::Changed::Layout;
        // Derive popup metrics from the same parsed JSON (single parse).
        return static_cast<Ui::Res::Type::Changed>(Common::Bit::Or(layoutChanged, loadPopupFrom(&j)));
    }
};

} // namespace Ui::Res::Store
