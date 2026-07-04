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
#include "common/noncopyable.h"
#include "common/unicode.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/fontrenderer.h"
#include "ui/gl/glrender.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/gl/svgrenderer.h"
#include "ui/gl/textalign.h"
#include "ui/interface/irenderer.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/changed.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/type.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Ui::Render {

// Global flag to enable/disable UI renderer debug output
constexpr bool UI_DEBUG = false;

/**
 * @brief OpenGL-based renderer for the UiLayout system
 *
 * Iterates UiElement list directly to produce GL draw calls.
 * Contains batched GL draw code (rounded rects, text, SVGs, borders).
 */
class UiRenderer final : public Ui::IRenderer, private Common::NonCopyable {
public:
    // Batched draw operation structs (same as HtmlRenderer, using native types)
    struct alignas(128) BGOp final {
        Ui::Res::Type::bound_t      bound;
        Ui::Res::Type::border_t     radius;
        int                         shadowOffsetX = 0;
        int                         shadowOffsetY = 0;
        int                         shadowBlur    = 0;
        bool                        hasShadow     = false;
        Ui::Res::Type::color_pair_t colors;
        Ui::Color                   shadowColor;
    };

    struct alignas(128) TextOp final {
        TextOp() = default;
        std::string            text;
        Ui::font_handle_t      hFont = 0;
        Ui::Color              color;
        Ui::Res::Type::bound_t pos;
        bool                   centered = false;
        fpx_t                  minPadH  = 0; // minimum horizontal padding (CSS px)
    };

    struct alignas(128) ImageOp final {
        std::string             src;
        Ui::Res::Type::bound_t  pos;
        Ui::Res::Type::border_t radius;
        Ui::Color               tint;
        bool                    isSvg   = false;
        bool                    tinted  = false;
        bool                    hovered = false;
        bool                    active  = false;
    };

    struct alignas(64) progress_t final {
        Ui::Res::Type::bound_t  tabRect;
        Ui::Res::Type::border_t tabRadius;
        fpx_t                   sectorW      = 0;
        int                     totalSectors = 0;
        int                     progress     = 0;
    };

    // makeCurrent: host callback that makes the target surface's GL context
    // current (the fw never touches a native window). width/height: initial
    // physical size; the host pushes later sizes via resize()/Render(w,h).
    UiRenderer(Ui::task_fn_t makeCurrent, fpx_t width, fpx_t height, const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
        , m_render(std::move(makeCurrent), resManager.resPath().fontDir())
        , m_width(width)
        , m_height(height)
    {
        std::cout << "[UiRenderer] Initialized - Scale: " << g_config.scale << "x"
                  << ", CSS viewport: " << clientWidth() << "x" << clientHeight() << std::endl;
    }

    ~UiRenderer() override = default;

    /**
     * @brief Build layout from current ResManager state
     *
     * Rebuilds the entire UiLayout from data structs.
     */
    void setContent()
    {
        m_layout.build(m_resManager.layout(),
                       m_resManager.theme(),
                       m_resManager.menus(),
                       m_resManager.buttons(),
                       m_resManager.tabBar(),
                       m_resManager.localeManager(),
                       clientWidth(),
                       clientHeight(),
                       &m_render,
                       m_resManager.statusText());

        // Re-apply hover/active state after layout rebuild (elements are recreated fresh)
        if (m_mouseX >= 0 && m_mouseY >= 0) {
            if (m_mouseDown) {
                UiElement * hit = m_layout.hitTest(m_mouseX, m_mouseY);
                if (hit != nullptr) {
                    hit->state = UiElementState::Active;
                }
            } else {
                updateHover(m_mouseX, m_mouseY);
            }
        }
    }

    bool render() override
    {
        Render(m_width, m_height);
        return true;
    }

    void resize(fpx_t w, fpx_t h) override
    {
        // Cache the new physical size; the layout rebuilds on the next
        // setContent() using clientWidth/clientHeight.
        m_width  = w;
        m_height = h;
    }

    void move() override
    { /* main window move needs no re-render */
    }

    [[nodiscard]] fpx_t width() const override { return m_width; }
    [[nodiscard]] fpx_t height() const override { return m_height; }
    void                cleanup() override { m_layout = UiLayout(); }

    void apply(Ui::Res::Type::Changed /*changed*/) override
    {
        // Content rebuild is handled by requestContentRefresh() -> setContent().
        // Not called here to avoid duplicate layout builds (SVG loading is expensive).
    }

    /**
     * @brief Get element bounds by ID
     * @return Element bounds, or empty Ui::Res::Type::bound_t{} if not found
     */
    [[nodiscard]] const Ui::Res::Type::bound_t & bound(id_t id)
    {
        static const Ui::Res::Type::bound_t empty {};
        UiElement *                         el = m_layout.elementById(id);
        return el != nullptr ? el->bound : empty;
    }

    /**
     * @brief Render frame at given physical pixel dimensions
     */
    void Render(fpx_t width, fpx_t height)
    {
        // Keep the cached size in step with the host's per-frame size.
        m_width  = width;
        m_height = height;

        // Produce draw ops from UiLayout elements, then let WindowManager
        // contribute extra ops (docks, future overlays) into the same op
        // buffers. The ops are then translated to IRender primitives.
        m_bgOps.clear();
        m_textOps.clear();
        m_imageOps.clear();
        m_progressOps.clear();

        generateDrawOps();
        if (m_extraOpsHook) {
            m_extraOpsHook(*this);
        }

        m_render.beginFrame(width, height);
        flushOps();
        m_render.endFrame();
    }

    // Caller can register a hook that runs between UiLayout-driven ops and
    // GL submission. The hook may call appendBg/appendImage to push extra
    // draws into the current frame. Used for dock columns (Path B): owned by
    // WindowManager, rendered alongside the UiElement-driven UI.
    void setExtraOpsHook(std::function<void(UiRenderer &)> hook) { m_extraOpsHook = std::move(hook); }

    // Public op-emitter helpers for hook users (e.g. DockColumn::render).
    // Plain colored rect with optional rounded corners; no shadow.
    //
    // elementColors is the natural theme block {fg = text/icon, bg = fill}.
    // parentBg is the surrounding area's bg, used for edge anti-aliasing in
    // Rounded's opaque-composite path. The SDF shader internally addresses
    // these as u_color (shape fill) and u_bgColor (AA blend), but that
    // naming is the shader's quirk, not something callers should know
    // about - we remap here so call sites stay theme-native.
    void appendBg(const Ui::Res::Type::bound_t &      rect,
                  const Ui::Res::Type::color_pair_t & elementColors,
                  const Ui::Color &                   parentBg,
                  const Ui::Res::Type::border_t &     radius = {})
    {
        BGOp op;
        op.bound     = rect;
        op.radius    = radius;
        op.colors.fg = elementColors.bg; // shape fill
        op.colors.bg = parentBg;         // AA blend color
        m_bgOps.emplace_back(op);
    }

    // Tinted SVG positioned in a rect. Caller computes the rect (SVG aspect
    // ratio is the caller's problem - for the dock grip we hand-pick a
    // centered sub-rect inside the grip strip).
    void appendImage(const Ui::Res::Type::bound_t & rect, const std::string & svgPath, Ui::Color tint)
    {
        ImageOp op;
        op.src    = svgPath;
        op.pos    = rect;
        op.tint   = tint;
        op.tinted = true;
        op.isSvg  = true;
        m_imageOps.emplace_back(op);
    }

    // === Ui::IEventApp Interface ===
    bool onMouseMove(int x, int y) override
    {
        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        const bool posChanged = (cssX != m_lastMouseCssX || cssY != m_lastMouseCssY);
        if (!posChanged) {
            return false;
        }
        m_lastMouseCssX = cssX;
        m_lastMouseCssY = cssY;
        m_mouseX        = cssX;
        m_mouseY        = cssY;

        const bool anyChanged = updateHover(cssX, cssY);

        // Fire hover callback for menu buttons. Detect by position (not Hovered
        // state) so a DISABLED top menu under the cursor is reported too - the
        // shell skips it (leaving any open menu open), which a Hovered scan would
        // miss since updateHover never marks a Disabled element Hovered.
        if (m_onElementHover) {
            id_t newHoverId = INVALID_ID;
            for (const auto & el : m_layout.elements()) {
                if (el.type == UiElementType::MenuButton && el.bound.contains(cssX, cssY)) {
                    newHoverId = el.id;
                    break;
                }
            }
            if (newHoverId != m_lastHoverId) {
                m_lastHoverId = newHoverId;
                if (newHoverId != INVALID_ID) {
                    m_onElementHover(UiElementType::MenuButton, newHoverId);
                }
            }
        }

        return anyChanged;
    }

    bool onMousePress(int x, int y, int /*clickCount*/) override
    {
        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        m_mouseDown = true;
        m_mouseX    = cssX;
        m_mouseY    = cssY;

        // Set active state
        UiElement * hit = m_layout.hitTest(cssX, cssY);
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active) {
                el.state = UiElementState::None;
            }
        }
        if (hit != nullptr) {
            hit->state = UiElementState::Active;
        }
        return hit != nullptr;
    }

    click_result_t onMouseRelease(int x, int y) override
    {
        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        m_mouseDown = false;
        m_mouseX    = cssX;
        m_mouseY    = cssY;

        click_result_t result;
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active && el.bound.contains(cssX, cssY) && el.id != INVALID_ID) {
                result.type = el.type;
                result.id   = el.id;
            }
            if (el.state == UiElementState::Active) {
                el.state       = UiElementState::None;
                result.changed = true;
            }
        }

        if (updateHover(cssX, cssY)) {
            result.changed = true;
        }

        return result;
    }

    bool onMouseLeave() override
    {
        m_mouseX    = -1;
        m_mouseY    = -1;
        m_mouseDown = false;

        bool changed = false;
        for (auto & el : m_layout.elements()) {
            if (el.state != UiElementState::Disabled && el.state != UiElementState::None) {
                el.state = UiElementState::None;
                changed  = true;
            }
        }
        return changed;
    }

    bool onScroll(int x, int y, fpx_t deltaY) override
    {
        (void)x;
        (void)y;
        (void)deltaY;
        return false;
    }

    // Callbacks for hover
    void setOnElementHover(std::function<void(UiElementType, id_t)> cb) { m_onElementHover = std::move(cb); }

    // Access to font renderer (for popup creation)
    Ui::Gl::FontRenderer * fontRenderer() { return m_render.fontRenderer(); }

private:
    bool updateHover(fpx_t cssX, fpx_t cssY)
    {
        bool anyChanged = false;
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active || el.state == UiElementState::Disabled) {
                continue;
            }
            const bool wasHovered = (el.state == UiElementState::Hovered);
            const bool isHovered  = el.bound.contains(cssX, cssY) && el.type != UiElementType::Text;
            el.state              = isHovered ? UiElementState::Hovered : UiElementState::None;
            if (isHovered != wasHovered) {
                anyChanged = true;
            }
        }

        return anyChanged;
    }

    [[nodiscard]] fpx_t clientWidth() const { return toCss(m_width); }
    [[nodiscard]] fpx_t clientHeight() const { return toCss(m_height); }

    /**
     * @brief Generate draw ops from UiLayout elements
     *
     * Iterates the flat element list and produces BGOp/TextOp/ImageOp directly.
     */
    void generateDrawOps()
    {
        const auto & theme  = m_resManager.theme();
        const auto & layout = m_resManager.layout();

        // ---- Region backgrounds (drawn before interactive elements) ----
        const fpx_t cssW     = clientWidth();
        const fpx_t cssH     = clientHeight();
        const fpx_t contentH = cssH - layout.topMenu.height - layout.statusBar.height;

        // Top menu bar
        addBgOp({ 0, 0, cssW, layout.topMenu.height }, theme.topMenu.bg);
        // Left toolbar
        addBgOp({ 0, layout.topMenu.height, layout.leftToolbar.width, contentH }, theme.leftToolbar.bg);
        // Right toolbar
        addBgOp({ cssW - layout.rightToolbar.width, layout.topMenu.height, layout.rightToolbar.width, contentH },
                theme.rightToolbar.bg);
        // Workspace tabs strip
        addBgOp({ layout.leftToolbar.width,
                  layout.topMenu.height,
                  cssW - layout.leftToolbar.width - layout.rightToolbar.width,
                  layout.workspaceTab.height },
                theme.workspaceTabs.bg);
        // Workspace area
        addBgOp({ layout.leftToolbar.width,
                  layout.topMenu.height + layout.workspaceTab.height,
                  cssW - layout.leftToolbar.width - layout.rightToolbar.width,
                  contentH - layout.workspaceTab.height },
                theme.workspace.bg);
        // Status bar
        addBgOp({ 0, cssH - layout.statusBar.height, cssW, layout.statusBar.height }, theme.statusBar.bg);

        // ---- Interactive elements ----
        const auto & menus     = m_resManager.menus();
        const auto & buttons   = m_resManager.buttons();
        const auto & tabBar    = m_resManager.tabBar();
        const auto & localeMgr = m_resManager.localeManager();

        for (const auto & el : m_layout.elements()) {
            // Resolve colors from ResManager based on element type + state
            const Ui::Res::Type::color_pair_t cp = resolveColors(el, theme);

            // Draw background (if visible and not inherited from parent)
            if (cp.bg.a() > 0 && !cp.bg.isInherit()) {
                BGOp op;
                op.bound     = el.bound;
                op.colors.fg = cp.bg;
                if (el.type == UiElementType::Tab) {
                    op.radius    = layout.workspaceTab.border;
                    op.colors.bg = theme.workspaceTabs.bg;
                } else if (el.type == UiElementType::TabClose) {
                    op.radius                = layout.tabCloseBorderRadius;
                    const UiElement * parent = m_layout.parentOf(el);
                    op.colors.bg             = (parent != nullptr) ? resolveColors(*parent, theme).bg : Ui::Color {};
                } else if (el.type == UiElementType::MenuButton) {
                    op.colors.bg = theme.topMenu.bg;
                    if (el.id == m_resManager.activeMenuId() || el.state == UiElementState::Active) {
                        op.radius = layout.menuButtonActiveBorder;
                    } else if (el.state == UiElementState::Hovered) {
                        op.radius = layout.menuButtonHoverBorder;
                    }
                }
                m_bgOps.emplace_back(op);
            }

            // Draw loading progress bar overlay on workspace tabs
            const Ui::tab_t * tab = (el.type == UiElementType::Tab) ? tabBar.find(el.id) : nullptr;
            if (tab != nullptr && tab->isLoading && tab->progress > 0) {
                const int   tlr          = roundToInt(layout.workspaceTab.border.topLeft);
                const int   totalSectors = (tlr > 0) ? std::max(2, static_cast<int>(el.bound.w) / tlr)
                                                     : layout.progressSectors;
                const fpx_t sectorW      = el.bound.w / totalSectors;
                // Scale model progress (0..layout.progressSectors) to visual sectors
                const int modelProgress = tab->progress;
                int       progress      = modelProgress * totalSectors / layout.progressSectors;
                if (modelProgress > 0 && progress < 1) {
                    progress = 1;
                }
                m_progressOps.push_back({ el.bound, layout.workspaceTab.border, sectorW, totalSectors, progress });
            }

            // Resolve font for this element type
            const Ui::font_handle_t elFont = resolveFont(el);

            // Resolve text from managers
            std::string text = resolveText(el, menus, buttons, tabBar, localeMgr, layout);

            // Draw text (if present)
            if (!text.empty() && elFont != 0) {
                const fpx_t pH = resolvePadH(el, layout);
                const fpx_t pV = resolvePadV(el, layout);
                TextOp      op;
                op.text     = std::move(text);
                op.hFont    = elFont;
                op.color    = cp.fg;
                op.centered = (el.type == UiElementType::MenuButton || el.type == UiElementType::Tab);
                if (op.centered) {
                    op.pos     = { el.bound.x, el.bound.y, el.bound.w, el.bound.h };
                    op.minPadH = pH;
                } else {
                    op.pos = { el.bound.x + pH, el.bound.y + pV, el.bound.w - pH * 2, el.bound.h - pV * 2 };
                }
                m_textOps.emplace_back(op);
            }

            // Resolve image from managers (toolbar buttons and icon menus)
            const std::string imageSrc = resolveImageSrc(el, buttons, menus);

            // Draw image (if present)
            if (!imageSrc.empty()) {
                ImageOp op;
                op.src            = imageSrc;
                op.isSvg          = isSvgFile(imageSrc);
                const fpx_t imgSz = (el.type == UiElementType::TabClose) ? layout.tabCloseIconSize
                                  : (el.type == UiElementType::TabArrow) ? el.bound.h
                                                                         : m_layout.buttonImgSize();
                fpx_t       imgW  = imgSz;
                fpx_t       imgH  = imgSz;
                if (op.isSvg) {
                    float svgW = 0;
                    float svgH = 0;
                    if (Ui::Gl::SvgRenderer::contentSizeIfCached(imageSrc, svgW, svgH) && svgW > 0 && svgH > 0) {
                        if (svgW >= svgH) {
                            imgW = imgSz;
                            imgH = std::max(1.0F, std::round(imgSz * svgH / svgW));
                        } else {
                            imgH = imgSz;
                            imgW = std::max(1.0F, std::round(imgSz * svgW / svgH));
                        }
                    }
                }
                const fpx_t imgX = el.bound.x + (el.bound.w - imgW) / 2.0F;
                const fpx_t imgY = el.bound.y + (el.bound.h - imgH) / 2.0F;
                op.pos           = { imgX, imgY, imgW, imgH };
                if (el.type == UiElementType::TabArrow) {
                    op.tinted = true;
                    op.tint   = theme.tabArrow.fg;
                } else if (el.type == UiElementType::TabClose) {
                    op.tinted = true;
                    op.tint   = cp.fg;
                }
                op.hovered = (el.state == UiElementState::Hovered);
                op.active  = (el.state == UiElementState::Active);
                m_imageOps.emplace_back(op);
            }
        }
    }

    // Resolve color pair for an element from ResManager theme
    [[nodiscard]] Ui::Res::Type::color_pair_t resolveColors(const UiElement &              el,
                                                            const Ui::Res::Type::theme_t & theme) const
    {
        switch (el.type) {
        case UiElementType::MenuButton: {
            if (el.state == UiElementState::Disabled) {
                return { theme.menuItemDisabledColor, theme.topMenuButton.bg };
            }
            // Action menus (icon buttons): no bg change on hover/active
            for (const auto & menu : m_resManager.menus()) {
                if (menu.id == el.id && !menu.actionKey.empty()) {
                    return theme.topMenuButton;
                }
            }
            const bool isOpenMenu = (el.id == m_resManager.activeMenuId());
            if (isOpenMenu || el.state == UiElementState::Active) {
                return theme.topMenuButtonActive;
            }
            if (el.state == UiElementState::Hovered) {
                return theme.topMenuButtonHover;
            }
            return theme.topMenuButton;
        }
        case UiElementType::Tab: {
            const Ui::tab_t * tab         = m_resManager.tabBar().find(el.id);
            const bool        isActiveTab = (tab != nullptr && tab->isActive);
            if (el.state == UiElementState::Active) {
                return theme.workspaceTabActive;
            }
            if (el.state == UiElementState::Hovered) {
                return theme.workspaceTabHover;
            }
            if (isActiveTab) {
                return { theme.workspaceTab.fg, theme.workspaceTabActive.bg };
            }
            return theme.workspaceTab;
        }
        case UiElementType::TabClose: {
            if (el.state == UiElementState::Hovered || el.state == UiElementState::Active) {
                return theme.tabCloseHover;
            }
            return theme.tabClose;
        }
        case UiElementType::TabArrow: {
            return theme.tabArrow;
        }
        case UiElementType::ToolbarButton: return theme.button;
        case UiElementType::Text:
            if (el.state == UiElementState::Active) {
                return theme.statusBarActive;
            }
            return theme.statusBar;
        default: return {};
        }
    }

    // Resolve font handle for an element from UiLayout cached handles
    [[nodiscard]] Ui::font_handle_t resolveFont(const UiElement & el) const
    {
        switch (el.type) {
        case UiElementType::MenuButton: return m_layout.menuFont();
        case UiElementType::Tab: {
            const Ui::tab_t * tab         = m_resManager.tabBar().find(el.id);
            const bool        isActiveTab = (tab != nullptr && tab->isActive);
            return isActiveTab ? m_layout.itemFontBold() : m_layout.itemFont();
        }
        case UiElementType::TabClose:
        case UiElementType::TabArrow: return 0;
        case UiElementType::Text: return m_layout.statusBarFont();
        default: return 0;
        }
    }

    // Resolve horizontal padding for an element from layout
    static fpx_t resolvePadH(const UiElement & el, const Ui::Res::Type::layout_t & layout)
    {
        switch (el.type) {
        case UiElementType::MenuButton: return layout.menuButtonPadH;
        case UiElementType::Tab: return layout.workspaceTab.padding;
        default: return 0;
        }
    }

    // Resolve vertical padding for an element from layout
    static fpx_t resolvePadV(const UiElement & /* el */, const Ui::Res::Type::layout_t & /* layout */) { return 0; }

    // Resolve display text for an element from managers
    std::string resolveText(const UiElement &                            el,
                            const std::vector<Ui::Res::Type::menu_t> &   menus,
                            const std::vector<Ui::Res::Type::button_t> & buttons,
                            const Ui::TabBar &                           tabBar,
                            const Ui::Res::LocaleManager &               localeMgr,
                            const Ui::Res::Type::layout_t &              layout) const
    {
        switch (el.type) {
        case UiElementType::MenuButton: {
            for (const auto & menu : menus) {
                if (menu.id == el.id) {
                    if (!menu.icon.empty()) {
                        return {};
                    }
                    return localeMgr.get(menu.label);
                }
            }
            return {};
        }
        case UiElementType::ToolbarButton: {
            for (const auto & btn : buttons) {
                if (btn.id == el.id) {
                    return localeMgr.get(btn.label);
                }
            }
            return {};
        }
        case UiElementType::Tab: {
            const Ui::tab_t * tab = tabBar.find(el.id);
            if (tab == nullptr) {
                return {};
            }
            auto        tabFont  = tab->isActive ? m_layout.itemFontBold() : m_layout.itemFont();
            const fpx_t textMaxW = el.bound.w - layout.workspaceTab.padding * 2 - layout.workspaceTab.height;
            return UiLayout::truncateFileName(tab->label, textMaxW, &m_render, tabFont);
        }
        case UiElementType::TabClose:
        case UiElementType::TabArrow: return {};
        case UiElementType::Text: return m_resManager.statusText();
        default: return {};
        }
    }

    // Resolve image source for an element from managers
    std::string resolveImageSrc(const UiElement &                            el,
                                const std::vector<Ui::Res::Type::button_t> & buttons,
                                const std::vector<Ui::Res::Type::menu_t> &   menus) const
    {
        if (el.type == UiElementType::MenuButton) {
            for (const auto & menu : menus) {
                if (menu.id == el.id && !menu.icon.empty()) {
                    return m_resManager.resPath().icon(m_resManager.themeIcon());
                }
            }
            return {};
        }
        if (el.type == UiElementType::TabClose) {
            const std::string & icon = m_resManager.tabCloseIcon();
            if (!icon.empty()) {
                return m_resManager.resPath().icon(icon);
            }
            return {};
        }
        if (el.type == UiElementType::TabArrow) {
            const std::string & icon = (el.id == TAB_ARROW_LEFT) ? m_resManager.tabArrowLeft()
                                                                 : m_resManager.tabArrowRight();
            return m_resManager.resPath().icon(icon);
        }
        if (el.type != UiElementType::ToolbarButton) {
            return {};
        }
        for (const auto & btn : buttons) {
            if (btn.id == el.id && !btn.icon.empty()) {
                if (btn.icon.find('/') == std::string::npos) {
                    return m_resManager.resPath().icon(btn.icon);
                }
                return btn.icon;
            }
        }
        return {};
    }

    // Add a simple background op (for region backgrounds)
    void addBgOp(const Ui::Res::Type::bound_t & bound, const Ui::Color & color)
    {
        if (color.a() > 0) {
            BGOp op;
            op.bound     = bound;
            op.colors.fg = color;
            m_bgOps.emplace_back(op);
        }
    }

    static bool isSvgFile(const std::string & src)
    {
        if (src.size() < 4) {
            return false;
        }
        std::string ext = src.substr(src.size() - 4);
        for (auto & c : ext) {
            c = tolower(static_cast<unsigned char>(c));
        }
        return ext == ".svg";
    }

    // === GL Drawing code (extracted from HtmlRenderer) ===

    // Translate the staged op vectors into IRender primitive calls. The bits
    // that read theme/layout (icon scale/shadow, progress colors) are resolved
    // here and passed as plain values, so the backend stays domain-blind. Order
    // (backgrounds -> progress -> images -> text) preserves the layering.
    void flushOps()
    {
        for (const auto & op : m_bgOps) {
            m_render.fillRect(op.bound, op.radius, op.colors, {});
        }

        for (const auto & p : m_progressOps) {
            drawProgress(p);
        }

        for (const auto & op : m_imageOps) {
            const fpx_t          scale = op.active ? m_resManager.layout().iconActiveScale : 1.0F;
            Ui::Render::shadow_t shadow {};
            if (op.hovered) {
                const auto & layout = m_resManager.layout();
                const auto & theme  = m_resManager.theme();
                shadow              = { layout.iconHoverShadowX,
                                        layout.iconHoverShadowY,
                                        layout.iconHoverShadowBlur,
                                        theme.shadowOpacity,
                                        theme.colorShadow };
            }
            // Transparent tint (a == 0) signals "not tinted" to the backend;
            // Ui::Color{} defaults to opaque, which would force the tinted path.
            const Ui::Color tint = op.tinted ? op.tint : Ui::Color::TransparentBlack();
            m_render.drawImage(op.src, op.pos, op.radius, tint, scale, shadow);
        }

        for (const auto & op : m_textOps) {
            m_render.drawText(op.hFont, op.text, op.pos, op.color, op.centered, op.minPadH);
        }
    }

    // Compose the tab loading bar from rounded-rect sectors plus the flat arrow
    // tip, all through IRender primitives. progress N (1..total): first sector
    // rounded (TL/BL), middle sectors plain, then the |> tip; at total, a fully
    // rounded bar.
    void drawProgress(const progress_t & p)
    {
        const int totalSectors = p.totalSectors;
        if (p.sectorW <= 0 || p.tabRect.h <= 0 || p.progress <= 0) {
            return;
        }

        const Ui::Res::Type::color_pair_t loadColors = { m_resManager.theme().colorLoad,
                                                         m_resManager.theme().workspaceTabs.bg };

        if (p.progress >= totalSectors) {
            m_render.fillRect({ p.tabRect.x, p.tabRect.y, p.sectorW * totalSectors, p.tabRect.h },
                              p.tabRadius,
                              loadColors,
                              {});
            return;
        }

        fpx_t curX = p.tabRect.x;

        const Ui::Res::Type::border_t firstRadius = { p.tabRadius.topLeft, 0, 0, p.tabRadius.bottomLeft };
        m_render.fillRect({ curX, p.tabRect.y, p.sectorW, p.tabRect.h }, firstRadius, loadColors, {});
        curX += p.sectorW;

        const int midCount = p.progress - 1;
        if (midCount > 0) {
            const fpx_t midW = p.sectorW * midCount;
            m_render.fillRect({ curX, p.tabRect.y, midW, p.tabRect.h }, {}, loadColors, {});
            curX += midW;
        }

        // Arrow tip |>. Snap to the physical pixel grid so fractional tab
        // geometry does not leave the tip at an arbitrary sub-pixel position.
        const fpx_t tipX = std::round(curX + p.sectorW);
        const fpx_t tipY = std::round(p.tabRect.y + p.tabRect.h / 2.0F);
        m_render.drawTriangle(curX, p.tabRect.y, tipX, tipY, curX, p.tabRect.y + p.tabRect.h, loadColors.fg);
    }

    // Core members
    UiLayout                    m_layout;
    const Ui::Res::ResManager & m_resManager;

    // GL implementation of the draw sink (owns the font/svg/rounded backends).
    // mutable: text measurement (a logically-const query) populates the glyph
    // atlas cache, so const resolve* methods can measure through it.
    mutable Ui::Gl::GlRender m_render;

    // Cached physical surface size (host pushes via ctor / resize() / Render()).
    fpx_t m_width  = 0;
    fpx_t m_height = 0;

    // Batched draw ops
    std::vector<BGOp>       m_bgOps {};
    std::vector<TextOp>     m_textOps {};
    std::vector<ImageOp>    m_imageOps {};
    std::vector<progress_t> m_progressOps {};

    // Extra-ops hook fires after generateDrawOps() and before the ops are
    // flushed, so callers can append BG/Image ops into the same frame (docks).
    std::function<void(UiRenderer &)> m_extraOpsHook;

    // Mouse state
    fpx_t m_mouseX        = -1;
    fpx_t m_mouseY        = -1;
    bool  m_mouseDown     = false;
    fpx_t m_lastMouseCssX = -1;
    fpx_t m_lastMouseCssY = -1;

    // Callbacks
    std::function<void(UiElementType, id_t)> m_onElementHover;
    id_t                                     m_lastHoverId = INVALID_ID;
};

} // namespace Ui::Render
