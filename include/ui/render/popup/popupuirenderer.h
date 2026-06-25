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

#include "ui/gl/textalign.h"
#include "ui/render/popup/popuprendererbase.h"
#include "ui/type.h"

#include <functional>
#include <iostream>
#include <set>

namespace Ui::Render::Popup {

/**
 * @brief Renderer for popup menus using Ui::Render::UiLayout system
 *
 * Thin wrapper around PopupRendererBase for popup windows.
 * Corner blending shader handles software transparency.
 */
class PopupUiRenderer final : public PopupRendererBase {
public:
    PopupUiRenderer(Ui::IWindow & window, const Ui::Res::ResManager & resManager, const Ui::Res::Type::menu_t & menu)
        : PopupRendererBase(window, resManager)
    {
        if (m_uiRender->fontRenderer() != nullptr) {
            m_popupFont   = m_uiRender->fontRenderer()->createFont(m_resManager.theme().menuItemFont);
            m_popupScFont = m_uiRender->fontRenderer()->createFont(m_resManager.theme().shortcutFont);

            // Bold variant of the popup item font, used to mark the active row
            // in radio-group popups (display mode / theme).
            Ui::Res::Type::font_t boldFont = m_resManager.theme().menuItemFont;
            boldFont.weight                = Ui::Res::Type::FontWeight::Bold;
            m_popupFontBold                = m_uiRender->fontRenderer()->createFont(boldFont);
        }

        m_popupElements   = Ui::Render::UiLayout::buildPopup(menu, m_resManager);
        m_containerBorder = m_resManager.layout().topMenuDropdown.border;
    }

    ~PopupUiRenderer() override { cleanup(); }

    PopupUiRenderer(const PopupUiRenderer &)             = delete;
    PopupUiRenderer(PopupUiRenderer &&)                  = delete;
    PopupUiRenderer & operator=(const PopupUiRenderer &) = delete;
    PopupUiRenderer & operator=(PopupUiRenderer &&)      = delete;

    using SubmenuHoverFn = std::function<
    void(const Ui::Res::Type::bound_t & bound, const Ui::Res::Type::menu_t & item, bool isFirst, bool isLast)>;

    void setSubmenuHoverCallback(SubmenuHoverFn fn) { m_onSubmenuHover = std::move(fn); }

    void setSubmenuParentId(id_t parentId)
    {
        if (m_submenuParentId == parentId) {
            return;
        }
        // The previous parent's state may have been set to Hovered solely
        // because it was the submenu parent. Without this cleanup the old
        // parent stays visually hovered even after its submenu is destroyed.
        if (m_submenuParentId != Ui::INVALID_ID) {
            for (auto & el : m_popupElements) {
                if (el.id == m_submenuParentId && el.state == Ui::Render::UiElementState::Hovered) {
                    el.state = Ui::Render::UiElementState::None;
                    break;
                }
            }
        }
        m_submenuParentId = parentId;
    }

    // Find a popup element by id (returns nullptr if not present). Used by
    // WindowManager when restoring submenu state after a resource reload.
    [[nodiscard]] const Ui::Render::UiElement * findElementById(id_t id) const
    {
        for (const auto & el : m_popupElements) {
            if (el.id == id) {
                return &el;
            }
        }
        return nullptr;
    }

    // Id of the item currently shown hovered (INVALID if none). Lets a resource
    // reload that recreates the popup preserve which item is highlighted.
    [[nodiscard]] id_t hoveredItemId() const
    {
        for (const auto & el : m_popupElements) {
            if (el.state == Ui::Render::UiElementState::Hovered) {
                return el.id;
            }
        }
        return Ui::INVALID_ID;
    }

    // Restore the hover highlight on a specific item after the popup is rebuilt.
    void setHoveredItem(id_t id)
    {
        for (auto & el : m_popupElements) {
            if (el.type != Ui::Render::UiElementType::MenuItem || el.state == Ui::Render::UiElementState::Disabled) {
                continue;
            }
            el.state = (el.id == id) ? Ui::Render::UiElementState::Hovered : Ui::Render::UiElementState::None;
        }
    }

    [[nodiscard]] bool isFirstElement(id_t id) const
    {
        return !m_popupElements.empty() && m_popupElements.front().id == id;
    }

    [[nodiscard]] bool isLastElement(id_t id) const
    {
        return !m_popupElements.empty() && m_popupElements.back().id == id;
    }

    void setContainerBorder(const Ui::Res::Type::border_t & border) { m_containerBorder = border; }

    void updateCorner(id_t cornerIndex, const std::vector<uint8_t> & pixels, fpx_t radius)
    {
        m_rounded.updateCorner(cornerIndex, pixels, radius);
    }

    bool render() override
    {
        const bool premultiplied = beginRender();
        renderPopupElements(premultiplied);
        return true;
    }

    void cleanup() override
    {
        m_popupElements.clear();
        PopupRendererBase::cleanup();
    }

    // === Ui::IEventApp Interface ===
    bool onMouseMove(int x, int y) override { return updatePopupHover(x, y); }

    bool onMouseLeave() override
    {
        for (auto & el : m_popupElements) {
            if (el.state != Ui::Render::UiElementState::Disabled && el.id != m_submenuParentId) {
                el.state = Ui::Render::UiElementState::None;
            }
        }
        return true;
    }

    bool onMousePress(int x, int y, int /*clickCount*/ = 1) override
    {
        const auto cssX   = toCss(x);
        const auto cssY   = toCss(y);
        bool       wasHit = false;

        for (auto & el : m_popupElements) {
            if (!wasHit && el.bound.contains(cssX, cssY) && el.type == Ui::Render::UiElementType::MenuItem
                && el.state != Ui::Render::UiElementState::Disabled && m_resManager.findMenuItem(el.id).items.empty()) {
                el.state = Ui::Render::UiElementState::Active;
                wasHit   = true;
            } else if (el.state == Ui::Render::UiElementState::Active) {
                el.state = Ui::Render::UiElementState::None;
            }
        }

        return wasHit;
    }

    Ui::Render::click_result_t onMouseRelease(int x, int y) override
    {
        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        Ui::Render::click_result_t result;

        for (auto & el : m_popupElements) {
            if (result.id == Ui::INVALID_ID && el.state == Ui::Render::UiElementState::Active
                && el.bound.contains(cssX, cssY) && el.type == Ui::Render::UiElementType::MenuItem
                && el.id != Ui::INVALID_ID) {
                result.type = el.type;
                result.id   = el.id;
            }
            if (el.state == Ui::Render::UiElementState::Active) {
                el.state = Ui::Render::UiElementState::None;
            }
        }

        return result;
    }

private:
    void renderPopupElements(bool premultiplied)
    {
        if (!m_uiRender || m_window == nullptr) {
            return;
        }

        const auto & theme     = m_resManager.theme();
        const auto & popup     = m_resManager.popup();
        const fpx_t  popupCssW = toCss(m_width);
        const fpx_t  popupCssH = toCss(m_height);

        glViewport(0, 0, static_cast<int>(m_width), static_cast<int>(m_height));

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        auto * fontRenderer = m_uiRender->fontRenderer();

        const Ui::Color bgColor = premultiplied ? Ui::Color::TransparentBlack() : theme.dropdown.bg;

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_rounded.begin(m_width, m_height, g_config.scale);

        // If the first/last menu item has its submenu open, the submenu pops
        // out to the right next to that edge of the parent. Flatten only the
        // matching corner (TR for first, BR for last) so the parent and the
        // submenu visually merge there. A single-item popup is both first and
        // last - it can flatten both at once.
        Ui::Res::Type::border_t containerBorder = m_containerBorder;
        if (m_submenuParentId != Ui::INVALID_ID) {
            id_t firstItemId = Ui::INVALID_ID;
            id_t lastItemId  = Ui::INVALID_ID;
            for (const auto & el : m_popupElements) {
                if (el.type != Ui::Render::UiElementType::MenuItem) {
                    continue;
                }
                if (firstItemId == Ui::INVALID_ID) {
                    firstItemId = el.id;
                }
                lastItemId = el.id;
            }
            if (firstItemId == m_submenuParentId) {
                containerBorder.topRight = 0;
            }
            if (lastItemId == m_submenuParentId) {
                containerBorder.bottomRight = 0;
            }
        }
        m_rounded.draw({ 0, 0, popupCssW, popupCssH }, containerBorder, { theme.dropdown.bg, bgColor });

        for (size_t i = 0; i < m_popupElements.size(); ++i) {
            const auto &                      el = m_popupElements[i];
            const Ui::Res::Type::color_pair_t cp = resolvePopupColors(el, theme);

            if (cp.bg.a() == 0) {
                continue;
            }

            Ui::Res::Type::border_t itemRadius;
            Ui::Res::Type::bound_t  itemBound = el.bound;
            if (el.type == Ui::Render::UiElementType::MenuItem
                && (el.state == Ui::Render::UiElementState::Hovered
                    || el.state == Ui::Render::UiElementState::Active)) {
                itemRadius          = m_resManager.layout().menuItemHoverBorder;
                const fpx_t marginH = m_resManager.layout().menuItemHoverMarginH;
                const fpx_t marginV = m_resManager.layout().menuItemHoverMarginV;
                itemBound.x += marginH;
                itemBound.y += marginV;
                itemBound.w -= marginH * 2;
                itemBound.h -= marginV * 2;
            } else {
                if (i == 0) {
                    itemRadius.topLeft  = containerBorder.topLeft;
                    itemRadius.topRight = containerBorder.topRight;
                }
                if (i == m_popupElements.size() - 1) {
                    itemRadius.bottomLeft  = containerBorder.bottomLeft;
                    itemRadius.bottomRight = containerBorder.bottomRight;
                }
            }
            m_rounded.draw(itemBound, itemRadius, { cp.bg, bgColor });

            // Theme preview swatch: split-color rounded shape (dark | light bg-main)
            // shown next to each theme submenu entry. Positioned within el.bound so
            // it stays put across hover (which shrinks itemBound).
            if (el.type == Ui::Render::UiElementType::MenuItem) {
                const Ui::Res::Type::menu_t menuItem = m_resManager.findMenuItem(el.id);
                if (menuItem.showsThemePreview) {
                    const Ui::Res::Type::theme_preview_t & themePreview = m_resManager.layout().themePreview;
                    const auto                   previewColors = m_resManager.themePreviewColors(menuItem.label);
                    const Ui::Res::Type::bound_t previewBound {
                        el.bound.x + el.bound.w - themePreview.right - themePreview.width,
                        el.bound.y + (el.bound.h - themePreview.height) / 2.0F,
                        themePreview.width,
                        themePreview.height,
                    };
                    m_rounded.drawSplit(previewBound,
                                        themePreview.border,
                                        previewColors.first.bg,  // dark  bg-main (left half)
                                        previewColors.second.bg, // light bg-main (right half)
                                        themePreview.splitAngle,
                                        bgColor);
                }
            }
        }
        Ui::Gl::Rounded::end();

        const fpx_t pH = popup.itemPaddingH;

        for (const auto & el : m_popupElements) {
            if (el.type != Ui::Render::UiElementType::MenuItem || m_popupFont == 0 || fontRenderer == nullptr) {
                continue;
            }

            const Ui::Res::Type::menu_t menuItem = m_resManager.findMenuItem(el.id);
            if (menuItem.id == Ui::INVALID_ID) {
                continue;
            }

            const std::string & text = m_resManager.localeManager().get(menuItem.label);
            if (text.empty()) {
                continue;
            }

            // Bold for the active radio choice: the item whose value (label)
            // equals its action's current value. No groups, no hardcoded names.
            const bool              isActive = m_resManager.isActiveItem(menuItem);
            const Ui::font_handle_t rowFont  = isActive && m_popupFontBold != 0 ? m_popupFontBold : m_popupFont;
            auto *                  fr       = fontRenderer->font(rowFont);
            if (fr == nullptr || fr->program == 0U) {
                continue;
            }

            const Ui::Res::Type::color_pair_t cp = resolvePopupColors(el, theme);

            // Label text
            {
                const auto startX   = Ui::Gl::TextAlign::startXLeft(el.bound, pH, g_config.scale);
                const auto baseline = fr->metrics.baselineCap(el.bound.y, el.bound.h, g_config.scale);
                if constexpr (Ui::Gl::LOG_TEXT_LAYOUT) {
                    static std::set<id_t> loggedIds;
                    if (loggedIds.insert(el.id).second) {
                        const auto bLine = fr->metrics.baseline(el.bound.y, el.bound.h, g_config.scale);
                        std::cout << "[Baseline] popup-item id=" << el.id << " text=\"" << text
                                  << "\" boxY=" << el.bound.y << " boxH=" << el.bound.h << " baselineCap=" << baseline
                                  << " baselineLineBox=" << bLine << " diff=" << (baseline - bLine) << std::endl;
                    }
                }
                auto verts = Ui::Gl::FontRenderer::buildTextVerts(*fr, text, startX, baseline);
                if (!verts.empty()) {
                    drawTextVerts(verts, cp.fg, *fr);
                }
            }

            // Helper: draw an SVG icon on the left or right of the row.
            const fpx_t iconSz      = m_resManager.layout().menuItemIconWidth;
            auto        drawRowIcon = [&](const std::string & iconName, Ui::Res::Type::IconPlace place) {
                if (iconName.empty()) {
                    return;
                }
                const std::string iconPath = m_resManager.resPath().icon(iconName);
                const std::string iconKey  = Ui::Gl::SvgRenderer::ensureLoaded(iconPath);
                if (iconKey.empty() || !Ui::Gl::SvgRenderer::isLoaded(iconKey)) {
                    return;
                }
                const fpx_t iconX = (place == Ui::Res::Type::IconPlace::Right ? el.bound.x + el.bound.w - pH - iconSz
                                                                                     : el.bound.x + pH)
                                  * g_config.scale;
                const fpx_t iconY = (el.bound.y + (el.bound.h - iconSz) / 2.0F) * g_config.scale;
                beginSvgDraw();
                m_svgRenderer.drawTinted(iconKey,
                                                { iconX, iconY, iconSz * g_config.scale, iconSz * g_config.scale },
                                         cp.fg);
                endSvgDraw();
            };

            // Item icon honours the per-item iconPlace (chevron right, info left, etc.)
            drawRowIcon(menuItem.icon, menuItem.iconPlace);

            // Theme preview swatch letters: "D" / "L" centered in each half,
            // colored with that variant's --cl-main so the swatch communicates
            // both bg and fg of each theme variant.
            if (menuItem.showsThemePreview && m_popupScFont != 0) {
                auto * shortcutFontRec = fontRenderer->font(m_popupScFont);
                if (shortcutFontRec != nullptr && shortcutFontRec->program != 0U) {
                    const Ui::Res::Type::theme_preview_t & themePreview = m_resManager.layout().themePreview;
                    const auto                   previewColors = m_resManager.themePreviewColors(menuItem.label);
                    const Ui::Res::Type::bound_t previewBound {
                        el.bound.x + el.bound.w - themePreview.right - themePreview.width,
                        el.bound.y + (el.bound.h - themePreview.height) / 2.0F,
                        themePreview.width,
                        themePreview.height,
                    };
                    const auto baseline   = shortcutFontRec->metrics.baselineCap(previewBound.y,
                                                                               previewBound.h,
                                                                               g_config.scale);
                    auto       drawLetter = [&](std::string_view glyph, fpx_t halfCenterCss, const Ui::Color & color) {
                        const auto  letterWidth = fontRenderer->textWidth(m_popupScFont, glyph);
                        const fpx_t startX      = (halfCenterCss - letterWidth / 2.0F) * g_config.scale;
                        auto verts = Ui::Gl::FontRenderer::buildTextVerts(*shortcutFontRec, glyph, startX, baseline);
                        if (!verts.empty()) {
                            drawTextVerts(verts, color, *shortcutFontRec);
                        }
                    };
                    drawLetter("Drk", previewBound.x + previewBound.w * 0.25F, previewColors.first.fg);
                    drawLetter("Lgt", previewBound.x + previewBound.w * 0.75F, previewColors.second.fg);
                }
            }

            // Shortcut text (right-aligned)
            if (!menuItem.shortcut.empty() && m_popupScFont != 0) {
                auto * scFr = fontRenderer->font(m_popupScFont);
                if (scFr != nullptr && scFr->program != 0U) {
                    const auto shortcutW = fontRenderer->textWidth(m_popupScFont, menuItem.shortcut);
                    const auto startX    = Ui::Gl::TextAlign::startXRight(el.bound, shortcutW, pH, g_config.scale);
                    const auto baseline  = scFr->metrics.baselineCap(el.bound.y, el.bound.h, g_config.scale);
                    auto       verts = Ui::Gl::FontRenderer::buildTextVerts(*scFr, menuItem.shortcut, startX, baseline);

                    if (!verts.empty()) {
                        const Ui::Color scColor = (el.state == Ui::Render::UiElementState::Hovered
                                                   || el.state == Ui::Render::UiElementState::Active)
                                                ? theme.shortcutHoverColor
                                                : theme.shortcutColor;
                        drawTextVerts(verts, scColor, *scFr);
                    }
                }
            }
        }
    }

    static Ui::Res::Type::color_pair_t resolvePopupColors(const Ui::Render::UiElement &  el,
                                                          const Ui::Res::Type::theme_t & theme)
    {
        switch (el.type) {
        case Ui::Render::UiElementType::MenuItem:
            if (el.state == Ui::Render::UiElementState::Active) {
                return theme.menuItemActive;
            }
            if (el.state == Ui::Render::UiElementState::Hovered) {
                return theme.menuItemHover;
            }
            if (el.state == Ui::Render::UiElementState::Disabled) {
                return { theme.menuItemDisabledColor, theme.menuItem.bg };
            }
            return theme.menuItem;
        case Ui::Render::UiElementType::Separator: return { {}, theme.separatorColor };
        default: return {};
        }
    }

    bool updatePopupHover(int physX, int physY)
    {
        const auto cssX = toCss(physX);
        const auto cssY = toCss(physY);

        bool changed       = false;
        bool submenuOpened = false;

        for (size_t i = 0; i < m_popupElements.size(); ++i) {
            auto & el = m_popupElements[i];
            if (el.state == Ui::Render::UiElementState::Active || el.state == Ui::Render::UiElementState::Disabled) {
                continue;
            }
            const bool wasHovered = (el.state == Ui::Render::UiElementState::Hovered);
            const bool isHovered  = el.bound.contains(cssX, cssY) && (el.type == Ui::Render::UiElementType::MenuItem);
            const bool isSubmenuParent = (m_submenuParentId != Ui::INVALID_ID && el.id == m_submenuParentId);
            el.state                   = (isHovered || isSubmenuParent) ? Ui::Render::UiElementState::Hovered
                                                                        : Ui::Render::UiElementState::None;
            if (isHovered != wasHovered) {
                changed = true;
            }

            if (isHovered && !wasHovered && m_onSubmenuHover) {
                const Ui::Res::Type::menu_t menuItem = m_resManager.findMenuItem(el.id);
                if (!menuItem.items.empty()) {
                    // Round CSS->physical via toPhysRound so the result stays divisible
                    // by g_config.scale. Popup layout accumulates fractional itemHeight
                    // (e.g. 31.32 CSS), so raw el.bound.y * g_config.scale produces odd
                    // physical pixels for items past index ~4, which becomes a
                    // half-point NSWindow origin on macOS and composites with a 1px
                    // AA band (visible as a dark line at the top of the submenu).
                    const Ui::Res::Type::bound_t physBound = { toPhysRound(el.bound.x),
                                                               toPhysRound(el.bound.y),
                                                               toPhysRound(el.bound.w),
                                                               toPhysRound(el.bound.h) };
                    const bool                   isFirst   = (i == 0);
                    const bool                   isLast    = (i == m_popupElements.size() - 1);
                    m_onSubmenuHover(physBound, menuItem, isFirst, isLast);
                    submenuOpened = true;
                }
            }
        }

        if (changed && !submenuOpened && m_onSubmenuHover) {
            m_onSubmenuHover({}, {}, false, false);
        }

        return changed;
    }

    std::vector<Ui::Render::UiElement> m_popupElements;
    Ui::font_handle_t                  m_popupFont     = 0;
    Ui::font_handle_t                  m_popupFontBold = 0;
    Ui::font_handle_t                  m_popupScFont   = 0;
    Ui::Res::Type::border_t            m_containerBorder;
    SubmenuHoverFn                     m_onSubmenuHover;
    id_t                               m_submenuParentId = Ui::INVALID_ID;
};

} // namespace Ui::Render::Popup
