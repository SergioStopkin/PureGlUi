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

#include "common/unicode.h"
#include "ui/codepoint.h"
#include "ui/convert.h"
#include "ui/gl/fontrenderer.h"
#include "ui/interface/irender.h"
#include "ui/render/uielement.h"
#include "ui/render/uielementstate.h"
#include "ui/res/resmanager.h"
#include "ui/tabbar.h"
#include "ui/type.h"

#include <string>
#include <vector>

namespace Ui::Render {

// A single UI element with layout and interaction state.
// Content (text, icons, shortcuts) and visual properties (colors, fonts)
// are NOT stored here -- renderers look them up from ResManager at render time.
struct alignas(32) UiElement final {
    UiElementType          type  = UiElementType::MenuButton;
    UiElementState         state = UiElementState::None;
    id_t                   id    = INVALID_ID;
    Ui::Res::Type::bound_t bound;
};

/**
 * @brief Lightweight layout engine for fixed-size UI regions
 *
 * Computes rectangles from Ui::Res::Type::layout_t/Ui::Res::Type::theme_t structs for the ~6 fixed-size
 * UI regions. Produces a flat list of UiElement for direct GL rendering.
 */
// Reserved element ID for status bar text (click-to-copy)
static constexpr id_t TAB_ARROW_LEFT  = 9997;
static constexpr id_t TAB_ARROW_RIGHT = 9998;
static constexpr id_t STATUS_TEXT_ID  = 9999;

class UiLayout final {
public:
    UiLayout() = default;

    /**
     * @brief Build the main window layout from resource data
     *
     * All coordinates are in CSS pixels. The renderer scales to physical pixels.
     *
     * @param layout       Layout dimensions from layout.json
     * @param theme        Theme colors from default-dark.json
     * @param menus        Top menu definitions
     * @param buttons      Toolbar button definitions
     * @param tabBar        Chrome tab model (projected workspace view)
     * @param localeManager Locale manager for string translation
     * @param windowCssW    Window width in CSS pixels
     * @param windowCssH    Window height in CSS pixels
     * @param render  Font renderer for text width measurement
     */
    void build(const Ui::Res::Type::layout_t &              layout,
               const Ui::Res::Type::theme_t &               theme,
               const std::vector<Ui::Res::Type::menu_t> &   menus,
               const std::vector<Ui::Res::Type::button_t> & buttons,
               const Ui::TabBar &                           tabBar,
               const Ui::Res::LocaleManager &               localeManager,
               fpx_t                                        windowCssW,
               fpx_t                                        windowCssH,
               Ui::IRender *                                render,
               const std::string &                          statusText = "")
    {
        m_elements.clear();
        m_menuFont      = 0;
        m_itemFont      = 0;
        m_itemFontBold  = 0;
        m_popupFont     = 0;
        m_statusBarFont = 0;
        m_buttonImgSize = layout.buttonImgSize;

        // Resolve font handles (create once, reuse)
        if (render != nullptr) {
            m_menuFont     = render->createFont(theme.topMenuFont);
            m_itemFont     = render->createFont(theme.workspaceTabFont);
            m_itemFontBold = render->createFont(
            { theme.workspaceTabFont.family, theme.workspaceTabFont.size, theme.workspaceTabActiveWeight });
            m_statusBarFont = render->createFont(theme.statusBarFont);
            m_popupFont     = m_itemFont;
        }

        // ---- Top menu buttons ----
        {
            const fpx_t padH    = layout.menuButtonPadH;
            fpx_t       cursorX = 0;

            // Left-aligned: regular menus (no direct action)
            for (const auto & menu : menus) {
                if (!menu.visible || !menu.actionKey.empty()) {
                    continue;
                }

                const std::wstring menuText = Common::Unicode::fromUtf8(localeManager.get(menu.label));
                fpx_t              textW    = 0;
                if (render != nullptr && m_menuFont != 0) {
                    textW = render->textWidth(m_menuFont, menuText);
                } else {
                    textW = static_cast<fpx_t>(menuText.size()) * layout.fallbackCharWidth;
                }
                const fpx_t buttonW = textW + padH * 2;

                auto & el = addElement(UiElementType::MenuButton,
                                       { cursorX, 0, buttonW, layout.topMenu.height },
                                       menu.id);
                if (!menu.enabled) {
                    el.state = UiElementState::Disabled;
                }

                cursorX += buttonW;
            }

            // Right-aligned: action menus (icon buttons, square)
            fpx_t rightCursor = windowCssW;
            auto  it          = menus.rbegin();
            while (it != menus.rend()) {
                if (it->visible && !it->actionKey.empty()) {
                    const fpx_t btnW = layout.topMenu.height; // square
                    rightCursor -= btnW;
                    auto & el = addElement(UiElementType::MenuButton,
                                           { rightCursor, 0, btnW, layout.topMenu.height },
                                           it->id);
                    if (!it->enabled) {
                        el.state = UiElementState::Disabled;
                    }
                }
                ++it;
            }
        }

        // ---- Left toolbar buttons ----
        {
            auto cursorY = layout.topMenu.height;

            for (const auto & button : buttons) {
                if (!button.visible) {
                    continue;
                }

                auto btnW = button.width > 0 ? button.width : layout.leftToolbar.width;
                auto btnH = button.height > 0 ? button.height : layout.leftToolbar.width;

                auto & btn = addElement(UiElementType::ToolbarButton, { 0, cursorY, btnW, btnH }, button.id);
                if (!button.enabled) {
                    btn.state = UiElementState::Disabled;
                }

                cursorY += btnH;
            }
        }

        // ---- Status bar text ----
        {
            const fpx_t barY = windowCssH - layout.statusBar.height;
            const fpx_t barH = layout.statusBar.height;
            const fpx_t padH = layout.statusBar.padding;

            if (!statusText.empty() && m_statusBarFont != 0) {
                const fpx_t textX = layout.leftToolbar.width + padH;
                const fpx_t textW = windowCssW - layout.leftToolbar.width - layout.rightToolbar.width - padH * 2;

                addElement(UiElementType::Text, { textX, barY, textW, barH }, STATUS_TEXT_ID);
            }
        }

        // ---- Workspace tabs ----
        {
            const fpx_t availW = windowCssW - layout.leftToolbar.width - layout.rightToolbar.width;
            buildWorkspaceTab(layout, tabBar, layout.leftToolbar.width, layout.topMenu.height, availW);
        }
    }

    /**
     * @brief Build popup layout from menu data
     *
     * @param menu        Menu definition with items
     * @param resManager  Resource manager for popup style and layout
     * @return Vector of UiElement for the popup items
     */
    static std::vector<UiElement> buildPopup(const Ui::Res::Type::menu_t & menu, const Ui::Res::ResManager & resManager)
    {
        const auto & popupStyle = resManager.popup();
        const fpx_t  popupCssW  = resManager.layout().topMenuDropdown.width;

        std::vector<UiElement> items {};

        fpx_t cursorY = 0;

        for (const auto & item : menu.items) {
            if (!item.visible) {
                continue;
            }

            if (item.separator) {
                UiElement sep;
                sep.type  = UiElementType::Separator;
                sep.bound = { popupStyle.separatorMarginH,
                              cursorY + popupStyle.separatorMarginV,
                              popupCssW - popupStyle.separatorMarginH * 2,
                              popupStyle.separatorHeight };
                items.emplace_back(sep);
                cursorY += popupStyle.separatorHeight + popupStyle.separatorMarginV * 2;
                continue;
            }

            UiElement el;
            el.type  = UiElementType::MenuItem;
            el.bound = { 0, cursorY, popupCssW, popupStyle.itemHeight };
            el.id    = item.id;
            if (!item.enabled) {
                el.state = UiElementState::Disabled;
            }
            items.emplace_back(el);

            cursorY += popupStyle.itemHeight;
        }

        return items;
    }

    /**
     * @brief Hit test: find the topmost element at CSS coordinates
     * @return Pointer to hit element, or nullptr if nothing hit
     */
    UiElement * hitTest(fpx_t cssX, fpx_t cssY)
    {
        // Iterate in reverse (last drawn = topmost). Disabled elements are inert -
        // not hit targets - so press/release/click skip them (matching hover).
        for (auto it = m_elements.rbegin(); it != m_elements.rend(); ++it) { // NOLINT(modernize-loop-convert)
            if (it->state != UiElementState::Disabled && it->bound.contains(cssX, cssY)) {
                return &(*it);
            }
        }
        return nullptr;
    }

    /**
     * @brief Find element by ID
     * @return Pointer to element, or nullptr if not found
     */
    UiElement * elementById(id_t id)
    {
        for (auto & el : m_elements) {
            if (el.id == id) {
                return &el;
            }
        }
        return nullptr;
    }

    [[nodiscard]] const UiElement * parentOf(const UiElement & child) const
    {
        for (const auto & el : m_elements) {
            if (el.id == child.id && el.type != child.type) {
                return &el;
            }
        }
        return nullptr;
    }

    /**
     * @brief Get all elements for rendering (flat list, draw in order)
     */
    [[nodiscard]] const std::vector<UiElement> & elements() const { return m_elements; }
    std::vector<UiElement> &                     elements() { return m_elements; }

    /**
     * @brief Get the font handle used for menu text
     */
    [[nodiscard]] Ui::font_handle_t menuFont() const { return m_menuFont; }

    /**
     * @brief Get the font handle used for item text (tab, popup items)
     */
    [[nodiscard]] Ui::font_handle_t itemFont() const { return m_itemFont; }

    /**
     * @brief Get the font handle used for bold item text (active tab)
     */
    [[nodiscard]] Ui::font_handle_t itemFontBold() const { return m_itemFontBold; }

    /**
     * @brief Get the font handle used for popup text
     */
    [[nodiscard]] Ui::font_handle_t popupFont() const { return m_popupFont; }

    /**
     * @brief Get the font handle used for status bar text
     */
    [[nodiscard]] Ui::font_handle_t statusBarFont() const { return m_statusBarFont; }

    /**
     * @brief Get the button image size in CSS pixels
     */
    [[nodiscard]] int buttonImgSize() const { return m_buttonImgSize; }

    // Fit a filename within maxW CSS pixels via a degradation ladder:
    //   1. full text                  if it fits
    //   2. prefix + ellipsis + ext    longest prefix that still leaves room
    //                                 for ellipsis + extension (preferred -
    //                                 keeps the file-type cue visible)
    //   3. prefix + ellipsis          step 2 can't fit even one prefix char;
    //                                 drop the ext and keep as much name as
    //                                 fits ("pyr...")
    //   4. ellipsis alone             nothing else fits
    //   5. ""                         even the ellipsis is wider than maxW
    // Returns the original text unchanged when measurement is unavailable
    // (null font renderer / handle = 0) so callers don't silently lose data.
    //
    // Works on std::wstring internally so substr() walks by codepoint, not
    // by byte. A UTF-8 std::string would slice multi-byte sequences in half
    // mid-loop and pass invalid bytes to textWidth(). Conversion happens at
    // the entry / exit boundary only.
    static std::string
    truncateFileName(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)
    {
        if (render == nullptr || font == 0) {
            return text;
        }
        if (maxW <= 0) {
            return {};
        }

        const std::wstring wtext = Common::Unicode::fromUtf8(text);
        if (render->textWidth(font, wtext) <= maxW) {
            return text;
        }

        // Split name and extension. Leading dot (".hidden") stays in the
        // name; dot > 0 enforces that.
        std::wstring wbase = wtext;
        std::wstring wext;
        const auto   dot = wtext.find_last_of(L'.');
        if (dot != std::wstring::npos && dot > 0) {
            wext  = wtext.substr(dot + 1);
            wbase = wtext.substr(0, dot);
        }

        // Single horizontal-three-dot glyph (~3-5 CSS) vs the three ASCII
        // dots it replaces (~8-12 CSS); the difference is what makes the
        // tight end of the ladder land on something useful. Named through
        // the Codepoint registry so it lives next to other UI-chrome glyphs.
        const std::wstring ellipsis = Ui::wstr(Ui::Codepoint::ThreeDotH);
        const auto         ellipW   = render->textWidth(font, ellipsis);

        // Step 2: prefix + ellipsis + ext.
        if (!wext.empty()) {
            const auto extW   = render->textWidth(font, wext);
            const auto needed = ellipW + extW;
            if (needed < maxW) {
                std::wstring best;
                for (size_t i = 1; i <= wbase.size(); ++i) {
                    const std::wstring candidate = wbase.substr(0, i);
                    if (render->textWidth(font, candidate) + needed > maxW) {
                        break;
                    }
                    best = candidate;
                }
                if (!best.empty()) {
                    return Common::Unicode::toUtf8(best + ellipsis + wext);
                }
            }
        }

        // Step 3: prefix + ellipsis (drop extension, keep recognisable name).
        {
            std::wstring best;
            for (size_t i = 1; i <= wbase.size(); ++i) {
                const std::wstring candidate = wbase.substr(0, i);
                if (render->textWidth(font, candidate) + ellipW > maxW) {
                    break;
                }
                best = candidate;
            }
            if (!best.empty()) {
                return Common::Unicode::toUtf8(best + ellipsis);
            }
        }

        // Step 4: ellipsis alone.
        if (ellipW <= maxW) {
            return Common::Unicode::toUtf8(ellipsis);
        }

        // Step 5: nothing fits.
        return {};
    }

private:
    std::vector<UiElement> m_elements {};
    Ui::font_handle_t      m_menuFont      = 0;
    Ui::font_handle_t      m_itemFont      = 0;
    Ui::font_handle_t      m_itemFontBold  = 0;
    Ui::font_handle_t      m_popupFont     = 0;
    Ui::font_handle_t      m_statusBarFont = 0;
    int                    m_buttonImgSize = 24;

    void addTabClose(fpx_t              tabX,
                     fpx_t              tabY,
                     fpx_t              tabW,
                     fpx_t              tabH,
                     fpx_t              closeMargin,
                     fpx_t              closeRight,
                     id_t               tabId,
                     const Ui::TabBar & tabBar)
    {
        const Ui::tab_t * tab = tabBar.find(tabId);
        if (tab == nullptr || !tab->hasContent) {
            return;
        }
        const fpx_t w = tabH - closeMargin * 2;
        const fpx_t h = w;
        const fpx_t x = tabX + tabW - w - closeRight;
        const fpx_t y = tabY + closeMargin;
        addElement(UiElementType::TabClose, { x, y, w, h }, tabId);
    }

    UiElement & addElement(UiElementType type, const Ui::Res::Type::bound_t & bound, id_t id = INVALID_ID)
    {
        UiElement el;
        el.type  = type;
        el.bound = bound;
        el.id    = id;
        m_elements.emplace_back(el);
        return m_elements.back();
    }

    // Build workspace tab elements with browser-like adaptive sizing:
    // 1. All tabs fit at max width -> use max width
    // 2. Tabs overflow at max but fit at min -> shrink tabs to fill available space
    // 3. Tabs overflow at min -> show arrows, shrink visible tabs to fill remaining space
    void buildWorkspaceTab(const Ui::Res::Type::layout_t & layout,
                           const Ui::TabBar &              tabBar,
                           fpx_t                           offsetX,
                           fpx_t                           offsetY,
                           fpx_t                           availW)
    {
        const fpx_t tabH    = layout.workspaceTab.height;
        const fpx_t maxTabW = layout.workspaceTab.width;
        const fpx_t minTabW = layout.tabMinWidth;
        const fpx_t arrowW  = layout.tabArrowWidth;
        const fpx_t closeM  = layout.tabCloseMargin;
        const fpx_t closeR  = layout.tabCloseRight;

        const std::vector<id_t> & tabIds = tabBar.order();
        const size_t              count  = tabIds.size();

        if (count == 0) {
            return;
        }

        // Case 1: all tabs fit at max width
        if (maxTabW * count <= availW) {
            fpx_t cursorX = offsetX;
            for (const id_t wsId : tabIds) {
                addElement(UiElementType::Tab, { cursorX, offsetY, maxTabW, tabH }, wsId);
                addTabClose(cursorX, offsetY, maxTabW, tabH, closeM, closeR, wsId, tabBar);
                cursorX += maxTabW;
            }
            return;
        }

        // Case 2: shrink tabs to fill -- all fit at min width or larger
        if (minTabW * count <= availW) {
            const fpx_t tabW    = availW / count;
            fpx_t       cursorX = offsetX;
            for (const id_t wsId : tabIds) {
                addElement(UiElementType::Tab, { cursorX, offsetY, tabW, tabH }, wsId);
                addTabClose(cursorX, offsetY, tabW, tabH, closeM, closeR, wsId, tabBar);
                cursorX += tabW;
            }
            return;
        }

        // Case 3: need arrows -- show visible subset, shrink to fill remaining space
        const size_t scrollOff    = tabBar.scrollOffset();
        const bool   showLeft     = (scrollOff > 0);
        fpx_t        tabAreaW     = availW - (showLeft ? arrowW : 0) - arrowW;
        size_t       visibleCount = std::max(static_cast<size_t>(tabAreaW / minTabW), static_cast<size_t>(1));
        bool         showRight    = (scrollOff + visibleCount < count);

        if (!showLeft) {
            tabAreaW     = availW - (showRight ? arrowW : 0);
            visibleCount = std::max(static_cast<size_t>(tabAreaW / minTabW), static_cast<size_t>(1));
            showRight    = (scrollOff + visibleCount < count);
        }

        if (scrollOff + visibleCount > count) {
            visibleCount = count - scrollOff;
        }

        tabAreaW = availW - (showLeft ? arrowW : 0) - (showRight ? arrowW : 0);

        const fpx_t tabW    = (visibleCount > 0) ? tabAreaW / visibleCount : minTabW;
        fpx_t       cursorX = offsetX;

        if (showLeft) {
            addElement(UiElementType::TabArrow, { cursorX, offsetY, arrowW, tabH }, TAB_ARROW_LEFT);
            cursorX += arrowW;
        }

        const size_t visibleEnd = std::min(scrollOff + visibleCount, count);
        for (size_t i = scrollOff; i < visibleEnd; ++i) {
            const id_t wsId = tabIds[i];
            addElement(UiElementType::Tab, { cursorX, offsetY, tabW, tabH }, wsId);
            addTabClose(cursorX, offsetY, tabW, tabH, closeM, closeR, wsId, tabBar);
            cursorX += tabW;
        }

        if (showRight) {
            const fpx_t arrowX = offsetX + availW - arrowW;
            addElement(UiElementType::TabArrow, { arrowX, offsetY, arrowW, tabH }, TAB_ARROW_RIGHT);
        }
    }
};

} // namespace Ui::Render
