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
#include "ui/res/key/cssprop.h"
#include "ui/res/key/element.h"
#include "ui/res/key/layout.h"
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

// Loads and holds res/layout.json: the layout_t geometry block plus the
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
        using Ui::Res::Key::CssPropKey;
        using Ui::Res::Key::cssPropKeyName;
        using Ui::Res::Key::ElementKey;
        using Ui::Res::Key::elementKeyName;

        const Ui::Res::Type::popup_t oldPopup = m_popup;

        // CSS string defaults
        std::string fontSize        = "14px";
        std::string padding         = "6px 16px";
        std::string lineHeight      = "1.38";
        std::string separatorHeight = "1px";
        std::string separatorMargin = "4px 0";

        if (j != nullptr) {
            // Load top-menu-item styling
            const std::string itemKey = elementKeyName(ElementKey::TopMenuItem);
            if (j->contains(itemKey) && (*j)[itemKey].is_object()) {
                const auto & item = (*j)[itemKey];
                fontSize          = item.value(cssPropKeyName(CssPropKey::FontSize), fontSize);
                padding           = item.value(cssPropKeyName(CssPropKey::Padding), padding);
                lineHeight        = item.value(cssPropKeyName(CssPropKey::LineHeight), lineHeight);
            }
            // Load separator styling
            const std::string sepKey = elementKeyName(ElementKey::TopMenuSeparator);
            if (j->contains(sepKey) && (*j)[sepKey].is_object()) {
                const auto & sep = (*j)[sepKey];
                separatorHeight  = sep.value(cssPropKeyName(CssPropKey::Height), separatorHeight);
                separatorMargin  = sep.value(cssPropKeyName(CssPropKey::Margin), separatorMargin);
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

    // Load res/layout.json once, deriving BOTH layout_t and popup_t from the
    // single parse. Returns the combined Changed bits. On file-load failure the
    // popup falls back to CSS defaults (layout is left untouched).
    Ui::Res::Type::Changed load(const std::string & file)
    {
        using Ui::Res::Key::CssPropKey;
        using Ui::Res::Key::cssPropKeyName;
        using Ui::Res::Key::ElementKey;
        using Ui::Res::Key::elementKeyName;
        using Ui::Res::Key::LayoutKey;
        using Ui::Res::Key::layoutKeyName;

        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return loadPopupFrom(nullptr);
        }

        const Ui::Res::Type::layout_t oldLayout = m_layout;

        // True when the named block exists and is an object.
        auto hasObject = [&j](ElementKey key) {
            const std::string name = elementKeyName(key);
            return j.contains(name) && j[name].is_object();
        };
        auto block = [&j](ElementKey key) -> const nlohmann::json & { return j[elementKeyName(key)]; };

        // Parse :root block for CSS variables
        std::unordered_map<std::string, std::string> cssVariables;
        if (hasObject(ElementKey::Root)) {
            const auto & root = block(ElementKey::Root);
            for (auto it = root.begin(); it != root.end(); ++it) {
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
            using Ui::Res::Key::CssPropKey;
            using Ui::Res::Key::cssPropKeyName;
            Ui::Res::Type::region_t region;
            region.height = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Height), "")));
            region.width = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Width), "")));
            region.margin = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Margin), "")));
            region.padding = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Padding), "")));
            region.top  = Ui::Convert::str2fpx(resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Top), "")));
            region.left = Ui::Convert::str2fpx(resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Left), "")));
            region.right = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Right), "")));
            region.bottom = Ui::Convert::str2fpx(
            resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::Bottom), "")));
            // Read CSS border-radius and parse into numeric border values
            const std::string brs = resolveVariable(regionJson.value(cssPropKeyName(CssPropKey::BorderRadius), ""));
            if (!brs.empty()) {
                region.border = Ui::Convert::parseCssBorderRadius(brs);
            }

            return region;
        };

        if (hasObject(ElementKey::TopMenu)) {
            m_layout.topMenu = parseRegion(block(ElementKey::TopMenu));
        }
        if (hasObject(ElementKey::LeftToolbar)) {
            m_layout.leftToolbar = parseRegion(block(ElementKey::LeftToolbar));
        }
        if (hasObject(ElementKey::RightToolbar)) {
            m_layout.rightToolbar = parseRegion(block(ElementKey::RightToolbar));
        }
        if (hasObject(ElementKey::StatusBar)) {
            m_layout.statusBar = parseRegion(block(ElementKey::StatusBar));
        }
        if (hasObject(ElementKey::Workspace)) {
            m_layout.workspace = parseRegion(block(ElementKey::Workspace));
        }
        if (hasObject(ElementKey::WorkspaceTab)) {
            const auto & wsTab    = block(ElementKey::WorkspaceTab);
            m_layout.workspaceTab = parseRegion(wsTab);
            m_layout.tabMinWidth  = Ui::Convert::str2fpx(
            resolveVariable(wsTab.value(cssPropKeyName(CssPropKey::MinWidth), "")));
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
        if (hasObject(ElementKey::WorkspaceTabClose)) {
            const auto & close      = block(ElementKey::WorkspaceTabClose);
            m_layout.tabCloseMargin = Ui::Convert::str2fpx(
            resolveVariable(close.value(cssPropKeyName(CssPropKey::Margin), "")));
            m_layout.tabCloseRight = Ui::Convert::str2fpx(
            resolveVariable(close.value(cssPropKeyName(CssPropKey::Right), "")));
            m_layout.tabCloseIconSize = Ui::Convert::str2fpx(
            resolveVariable(close.value(cssPropKeyName(CssPropKey::Height), "")));
            m_layout.tabCloseBorderRadius = Ui::Convert::parseCssBorderRadius(
            resolveVariable(close.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.tabCloseIcon = Common::Sanitize::filePath(
            resolveVariable(close.value(cssPropKeyName(CssPropKey::Icon), "var(--close-icon)")),
            "layout.tabCloseIcon");
        }

        // Parse workspace-tab-arrow layout (icon filenames + dimensions)
        if (hasObject(ElementKey::WorkspaceTabArrow)) {
            const auto & arrow     = block(ElementKey::WorkspaceTabArrow);
            m_layout.tabArrowWidth = Ui::Convert::str2fpx(
            resolveVariable(arrow.value(cssPropKeyName(CssPropKey::Width), "")));
            m_layout.tabArrowHeight = Ui::Convert::str2fpx(
            resolveVariable(arrow.value(cssPropKeyName(CssPropKey::Height), "")));
            m_layout.tabArrowIconLeft = Common::Sanitize::filePath(
            arrow.value(cssPropKeyName(CssPropKey::IconLeft), m_layout.tabArrowIconLeft),
            "layout.tabArrowIconLeft");
            m_layout.tabArrowIconRight = Common::Sanitize::filePath(
            arrow.value(cssPropKeyName(CssPropKey::IconRight), m_layout.tabArrowIconRight),
            "layout.tabArrowIconRight");
        }

        // Parse top-menu-dropdown region used for popup styling (border-radius etc.)
        if (hasObject(ElementKey::TopMenuDropdown)) {
            m_layout.topMenuDropdown = parseRegion(block(ElementKey::TopMenuDropdown));
        }

        // Parse theme-preview swatch (rendered next to each theme name in submenu).
        // Layout only; per-theme colors are populated by scanThemeNames.
        if (hasObject(ElementKey::ThemePreview)) {
            const auto & tp             = block(ElementKey::ThemePreview);
            m_layout.themePreview.width = Ui::Convert::str2fpx(
            resolveVariable(tp.value(cssPropKeyName(CssPropKey::Width), "")));
            m_layout.themePreview.height = Ui::Convert::str2fpx(
            resolveVariable(tp.value(cssPropKeyName(CssPropKey::Height), "")));
            m_layout.themePreview.border = Ui::Convert::parseCssBorderRadius(
            resolveVariable(tp.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.themePreview.right = Ui::Convert::str2fpx(
            resolveVariable(tp.value(cssPropKeyName(CssPropKey::Right), "")));
            m_layout.themePreview.splitAngle = Ui::Convert::parseCssNumber(
            resolveVariable(tp.value(cssPropKeyName(CssPropKey::SplitAngle), "45")));
        }

        // Parse element-specific layout values
        if (hasObject(ElementKey::TopMenuButtonLabel)) {
            const auto &      btn = block(ElementKey::TopMenuButtonLabel);
            const std::string pad = btn.value(cssPropKeyName(CssPropKey::Padding), "0 16px");
            auto              sp  = pad.find(' ');
            if (sp != std::string::npos) {
                m_layout.menuButtonPadH = Ui::Convert::str2fpx(pad.substr(sp + 1));
            }
        }
        if (hasObject(ElementKey::TopMenuButtonLabelHover)) {
            const std::string brs = resolveVariable(
            block(ElementKey::TopMenuButtonLabelHover).value(cssPropKeyName(CssPropKey::BorderRadius), ""));
            if (!brs.empty()) {
                m_layout.menuButtonHoverBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
        }
        if (hasObject(ElementKey::TopMenuButtonLabelActive)) {
            const std::string brs = resolveVariable(
            block(ElementKey::TopMenuButtonLabelActive).value(cssPropKeyName(CssPropKey::BorderRadius), ""));
            if (!brs.empty()) {
                m_layout.menuButtonActiveBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
        }
        if (hasObject(ElementKey::TopMenuItemHover)) {
            const auto &      itemHover = block(ElementKey::TopMenuItemHover);
            const std::string brs = resolveVariable(itemHover.value(cssPropKeyName(CssPropKey::BorderRadius), ""));
            if (!brs.empty()) {
                m_layout.menuItemHoverBorder = Ui::Convert::parseCssBorderRadius(brs);
            }
            const std::string margin = itemHover.value(cssPropKeyName(CssPropKey::Margin), "");
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
        if (hasObject(ElementKey::TopMenuItemIcon)) {
            const auto &      itemIcon = block(ElementKey::TopMenuItemIcon);
            const std::string width    = resolveVariable(itemIcon.value(cssPropKeyName(CssPropKey::Width), ""));
            if (!width.empty()) {
                m_layout.menuItemIconWidth = Ui::Convert::parseCssNumber(width);
            }
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::ButtonImgSize))) {
            m_layout.buttonImgSize = Ui::Convert::str2int(cssVariables[layoutKeyName(LayoutKey::ButtonImgSize)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::WindowWidth))) {
            m_layout.windowWidth = Ui::Convert::str2fpx(cssVariables[layoutKeyName(LayoutKey::WindowWidth)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::WindowHeight))) {
            m_layout.windowHeight = Ui::Convert::str2fpx(cssVariables[layoutKeyName(LayoutKey::WindowHeight)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::ButtonIconHoverShadowX))) {
            m_layout.iconHoverShadowX = Ui::Convert::parseCssNumber(
            cssVariables[layoutKeyName(LayoutKey::ButtonIconHoverShadowX)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::ButtonIconHoverShadowY))) {
            m_layout.iconHoverShadowY = Ui::Convert::parseCssNumber(
            cssVariables[layoutKeyName(LayoutKey::ButtonIconHoverShadowY)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::ButtonIconHoverShadowBlur))) {
            m_layout.iconHoverShadowBlur = Ui::Convert::parseCssNumber(
            cssVariables[layoutKeyName(LayoutKey::ButtonIconHoverShadowBlur)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::ButtonIconActiveScale))) {
            m_layout.iconActiveScale = Ui::Convert::parseCssNumber(
            cssVariables[layoutKeyName(LayoutKey::ButtonIconActiveScale)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::FallbackCharWidth))) {
            m_layout.fallbackCharWidth = Ui::Convert::str2fpx(
            cssVariables[layoutKeyName(LayoutKey::FallbackCharWidth)]);
        }
        if (cssVariables.contains(layoutKeyName(LayoutKey::MenuMaxDepth))) {
            // Nonpositive/malformed value keeps the default: a cap of 0 would
            // drop every submenu at the first descent.
            const int menuMaxDepth = Ui::Convert::str2int(cssVariables[layoutKeyName(LayoutKey::MenuMaxDepth)]);
            if (menuMaxDepth > 0) {
                m_layout.menuMaxDepth = menuMaxDepth;
            }
        }

        // Parse dialog layout
        if (hasObject(ElementKey::Dialog)) {
            m_layout.dialog = parseRegion(block(ElementKey::Dialog));
        }

        if (hasObject(ElementKey::DialogTitle)) {
            const auto & dt            = block(ElementKey::DialogTitle);
            m_layout.dialogTitleHeight = Ui::Convert::parseCssNumber(
            resolveVariable(dt.value(cssPropKeyName(CssPropKey::Height), "24px")));
            m_layout.dialogTitleMargin = Ui::Convert::parseCssNumber(
            resolveVariable(dt.value(cssPropKeyName(CssPropKey::MarginBottom), "")));
        }

        if (hasObject(ElementKey::DialogText)) {
            const auto & dtxt         = block(ElementKey::DialogText);
            m_layout.dialogTextMargin = Ui::Convert::parseCssNumber(
            resolveVariable(dtxt.value(cssPropKeyName(CssPropKey::MarginBottom), "")));
        }

        if (hasObject(ElementKey::DialogIcon)) {
            const auto & di     = block(ElementKey::DialogIcon);
            m_layout.dialogIcon = {
                Ui::Convert::parseCssNumber(resolveVariable(di.value(cssPropKeyName(CssPropKey::Left), "16px"))),
                Ui::Convert::parseCssNumber(resolveVariable(di.value(cssPropKeyName(CssPropKey::Top), "16px"))),
                Ui::Convert::parseCssNumber(resolveVariable(di.value(cssPropKeyName(CssPropKey::Width), "20px"))),
                Ui::Convert::parseCssNumber(resolveVariable(di.value(cssPropKeyName(CssPropKey::Height), "20px")))
            };
        }

        if (hasObject(ElementKey::DialogClose)) {
            const auto & dialogClose = block(ElementKey::DialogClose);
            m_layout.dialogCloseSize = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::Height), "12px")));
            m_layout.dialogCloseBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.dialogCloseMargin = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::Margin), "2px")));
            m_layout.dialogCloseTop = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::Top), "8px")));
            m_layout.dialogCloseRight = Ui::Convert::parseCssNumber(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::Right), "8px")));
            m_layout.dialogCloseIcon = Common::Sanitize::filePath(
            resolveVariable(dialogClose.value(cssPropKeyName(CssPropKey::Icon), "var(--close-icon)")),
            "layout.dialogCloseIcon");
        }

        if (hasObject(ElementKey::DialogButton)) {
            const auto & dialogButton = block(ElementKey::DialogButton);
            m_layout.dialogButtonH    = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value(cssPropKeyName(CssPropKey::Height), "28px")));
            m_layout.dialogButtonBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogButton.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.dialogButtonPad = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value(cssPropKeyName(CssPropKey::Padding), "8px")));
            m_layout.dialogButtonMinW = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value(cssPropKeyName(CssPropKey::MinWidth), "70px")));
            m_layout.dialogButtonShift = Ui::Convert::parseCssNumber(
            resolveVariable(dialogButton.value(cssPropKeyName(CssPropKey::Shift), "")));
        }

        if (hasObject(ElementKey::DialogScrollbar)) {
            const auto & dialogScrollbar = block(ElementKey::DialogScrollbar);
            m_layout.dialogScrollbarW    = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value(cssPropKeyName(CssPropKey::Width), "")));
            m_layout.dialogScrollbarRight = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value(cssPropKeyName(CssPropKey::Right), "")));
            m_layout.dialogScrollbarBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(dialogScrollbar.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.dialogScrollbarMinThumb = Ui::Convert::parseCssNumber(
            resolveVariable(dialogScrollbar.value(cssPropKeyName(CssPropKey::MinThumbHeight), "")));
        }

        if (hasObject(ElementKey::DialogScrollbarHover)) {
            const auto & scrollbarHover    = block(ElementKey::DialogScrollbarHover);
            m_layout.dialogScrollbarHoverW = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value(cssPropKeyName(CssPropKey::Width), "")));
            m_layout.dialogScrollbarHoverRight = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value(cssPropKeyName(CssPropKey::Right), "")));
            m_layout.dialogScrollbarHoverBorder = Ui::Convert::parseCssBorderRadius(
            resolveVariable(scrollbarHover.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.dialogScrollbarHoverMinThumb = Ui::Convert::parseCssNumber(
            resolveVariable(scrollbarHover.value(cssPropKeyName(CssPropKey::MinThumbHeight), "")));
        }

        // Shared dock dimensional tunables. One block applies to every dock
        // instance; per-dock state (current width, collapsed flag) lives in
        // session, not layout.json.
        if (hasObject(ElementKey::DockDefaults)) {
            const auto & dd                 = block(ElementKey::DockDefaults);
            m_layout.dockDefaults.gripWidth = Ui::Convert::str2fpx(
            resolveVariable(dd.value(cssPropKeyName(CssPropKey::GripWidth), "")));
            m_layout.dockDefaults.gripRadius = Ui::Convert::str2fpx(
            resolveVariable(dd.value(cssPropKeyName(CssPropKey::BorderRadius), "")));
            m_layout.dockDefaults.clickThreshold = Ui::Convert::str2fpx(
            resolveVariable(dd.value(cssPropKeyName(CssPropKey::ClickThreshold), "")));
            m_layout.dockDefaults.gripIcon = Common::Sanitize::filePath(
            resolveVariable(dd.value(cssPropKeyName(CssPropKey::GripIcon), "")),
            "dock.gripIcon");
        }

        const Ui::Res::Type::Changed layoutChanged = (m_layout == oldLayout) ? Ui::Res::Type::Changed::None
                                                                             : Ui::Res::Type::Changed::Layout;
        // Derive popup metrics from the same parsed JSON (single parse).
        return static_cast<Ui::Res::Type::Changed>(Common::Bit::Or(layoutChanged, loadPopupFrom(&j)));
    }
};

} // namespace Ui::Res::Store
