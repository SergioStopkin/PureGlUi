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
#include "ui/elementid.h"
#include "ui/gl/fontrenderer.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/gl/svgrenderer.h"
#include "ui/gl/textalign.h"
#include "ui/interface/irender.h"
#include "ui/interface/irenderer.h"
#include "ui/render/elementstyle.h"
#include "ui/render/truncate.h"
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
        Ui::Res::Type::AlignH  alignH  = Ui::Res::Type::AlignH::Left;
        Ui::Res::Type::AlignV  alignV  = Ui::Res::Type::AlignV::Center;
        fpx_t                  minPadH = 0; // minimum horizontal padding (CSS px)
    };

    struct alignas(128) ImageOp final {
        std::string             src;
        Ui::Res::Type::bound_t  pos;
        Ui::Res::Type::border_t radius;
        Ui::Color               tint;
        bool                    isSvg    = false;
        bool                    tinted   = false;
        bool                    hovered  = false;
        bool                    active   = false;
        bool                    isFilled = false; // true draws outline art as a solid shape
    };

    struct alignas(64) progress_t final {
        Ui::Res::Type::bound_t  tabRect;
        Ui::Res::Type::border_t tabRadius;
        fpx_t                   sectorW      = 0;
        int                     totalSectors = 0;
        int                     progress     = 0;
    };

    // render: the draw sink, injected so the fw renderer is decoupled from the GL
    // backend - GlRender in the app, a fake sink in headless tests. width/height:
    // initial physical size; the host pushes later sizes via resize()/Render(w,h).
    UiRenderer(std::unique_ptr<Ui::IRender> render, fpx_t width, fpx_t height, const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
        , m_render(std::move(render))
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
                       m_render.get(),
                       m_resManager.statusText());

        // Before the state re-apply below, so contributed elements can pick up
        // hover/active too
        if (m_extraElementsHook) {
            m_extraElementsHook(m_layout);
        }

        // Re-apply hover/active state after layout rebuild (elements are recreated fresh)
        if (m_mouseX >= 0 && m_mouseY >= 0) {
            if (m_mouseDown) {
                UiElement * hit = m_layout.hitTest(m_mouseX, m_mouseY, EventKind::LeftClick);
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

    void cleanup() override { m_layout = UiLayout(); }

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

        m_render->beginFrame(width, height);
        flushOps();
        m_render->endFrame();
    }

    // Caller can register a hook that runs between UiLayout-driven ops and
    // GL submission. The hook may call appendBg/appendImage to push extra
    // draws into the current frame. Used for dock columns (Path B): owned by
    // WindowManager, rendered alongside the UiElement-driven UI.
    void setExtraOpsHook(std::function<void(UiRenderer &)> hook) { m_extraOpsHook = std::move(hook); }

    // Contributors of elements the layout does not build itself, run right after
    // build() so their elements land on top. The counterpart to the ops hook:
    // that one draws dock chrome, this one makes it hit-testable.
    void setExtraElementsHook(std::function<void(UiLayout &)> hook) { m_extraElementsHook = std::move(hook); }

    // Topmost element accepting this event at this point, or null. Lets the
    // coordinator resolve a capture target from the declared masks instead of
    // testing each widget's geometry by hand.
    [[nodiscard]] UiElement * hitTest(fpx_t cssX, fpx_t cssY, EventKind event)
    {
        return m_layout.hitTest(cssX, cssY, event);
    }

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

    // Text in a rect, left-aligned on the rect's baseline-capped centre, or
    // centred when `isCentered`. The rect is the clip/layout box, not the glyph
    // extent - a caller laying out rows passes the row rect and lets the text
    // sit inside it.
    void appendText(const Ui::Res::Type::bound_t & rect,
                    const std::string &            text,
                    Ui::font_handle_t              font,
                    const Ui::Color &              color,
                    Ui::Res::Type::AlignH          alignH  = Ui::Res::Type::AlignH::Left,
                    Ui::Res::Type::AlignV          alignV  = Ui::Res::Type::AlignV::Center,
                    fpx_t                          minPadH = 0)
    {
        if (text.empty() || font == 0) {
            return;
        }
        TextOp op;
        op.text    = text;
        op.hFont   = font;
        op.color   = color;
        op.pos     = rect;
        op.alignH  = alignH;
        op.alignV  = alignV;
        op.minPadH = minPadH;
        m_textOps.emplace_back(op);
    }

    // Fonts a hook user can lay text out with. The item font is the row font;
    // bold marks an active row the way it marks an active tab.
    [[nodiscard]] Ui::font_handle_t itemFont() const { return m_layout.itemFont(); }
    [[nodiscard]] Ui::font_handle_t itemFontBold() const { return m_layout.itemFontBold(); }

    // So a hook user can right-align or truncate before emitting
    [[nodiscard]] fpx_t textWidth(Ui::font_handle_t font, const std::string & text) const
    {
        return m_render->textWidth(font, Common::Unicode::fromUtf8(text));
    }

    // Nothing clips a draw op, so text that must stay inside its box is cut here
    [[nodiscard]] std::string truncate(Ui::font_handle_t font, const std::string & text, fpx_t maxW) const
    {
        return Ui::Render::truncateText(text, maxW, m_render.get(), font);
    }

    // Tinted SVG positioned in a rect. Caller computes the rect (SVG aspect
    // ratio is the caller's problem - for the dock grip we hand-pick a
    // centered sub-rect inside the grip strip).
    void
    appendImage(const Ui::Res::Type::bound_t & rect, const std::string & svgPath, Ui::Color tint, bool isFilled = false)
    {
        ImageOp op;
        op.src      = svgPath;
        op.pos      = rect;
        op.tint     = tint;
        op.tinted   = true;
        op.isSvg    = true;
        op.isFilled = isFilled;
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

    // Chrome activates on Left only. Middle and Right now reach here because
    // the dispatcher stopped filtering them - they belong to content surfaces
    element_event_t onMousePress(int                     x,
                                 int                     y,
                                 Ui::Window::MouseButton button,
                                 int /*clickCount*/,
                                 Ui::Window::KeyModifier /*modifiers*/) override
    {
        if (button != Ui::Window::MouseButton::Left) {
            return {};
        }

        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        m_mouseDown = true;
        m_mouseX    = cssX;
        m_mouseY    = cssY;

        // Set active state
        UiElement * hit = m_layout.hitTest(cssX, cssY, EventKind::LeftClick);
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active) {
                el.state = UiElementState::None;
            }
        }

        element_event_t result;
        result.x = cssX;
        result.y = cssY;
        if (hit != nullptr) {
            hit->state     = UiElementState::Active;
            result.type    = hit->type;
            result.id      = hit->id;
            result.event   = EventKind::LeftClick;
            result.changed = true;
        }
        return result;
    }

    element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button) override
    {
        if (button != Ui::Window::MouseButton::Left) {
            return {};
        }

        const auto cssX = toCss(x);
        const auto cssY = toCss(y);

        m_mouseDown = false;
        m_mouseX    = cssX;
        m_mouseY    = cssY;

        element_event_t result;
        result.x = cssX;
        result.y = cssY;
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active && el.bound.contains(cssX, cssY) && el.id != INVALID_ID) {
                result.type  = el.type;
                result.id    = el.id;
                result.event = EventKind::LeftClick;
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

    element_event_t onScroll(int x, int y, fpx_t deltaY) override
    {
        (void)x;
        (void)y;
        (void)deltaY;
        return {};
    }

    // Callbacks for hover
    void setOnElementHover(std::function<void(UiElementType, id_t)> cb) { m_onElementHover = std::move(cb); }

private:
    bool updateHover(fpx_t cssX, fpx_t cssY)
    {
        bool anyChanged = false;
        for (auto & el : m_layout.elements()) {
            if (el.state == UiElementState::Active || el.state == UiElementState::Disabled) {
                continue;
            }
            const bool wasHovered = (el.state == UiElementState::Hovered);
            const bool isHovered  = el.bound.contains(cssX, cssY) && acceptsEvent(el.accepts, EventKind::Hover);
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
        const auto & tabBar = m_resManager.tabBar();

        for (const auto & el : m_layout.elements()) {
            element_style_t style = resolveStyle(el, theme);

            // Draw background (if visible and not inherited from parent)
            if (style.colors.bg.a() > 0 && !style.colors.bg.isInherit()) {
                BGOp op;
                op.bound     = el.bound;
                op.colors.fg = style.colors.bg;
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

            // Draw text (if present)
            if (!style.text.empty() && style.font != 0) {
                const fpx_t pH = style.padH;
                TextOp      op;
                op.text  = std::move(style.text);
                op.hFont = style.font;
                op.color = style.colors.fg;

                // A menu button and a tab centre while their label fits and
                // fall back to left-aligned when it does not, so a long file
                // name does not spill out of the left edge
                const bool isCentered = (el.type == UiElementType::MenuButton || el.type == UiElementType::Tab);
                op.alignH             = isCentered ? Ui::Res::Type::AlignH::CenterClamped : Ui::Res::Type::AlignH::Left;
                if (isCentered) {
                    op.pos     = { el.bound.x, el.bound.y, el.bound.w, el.bound.h };
                    op.minPadH = pH;
                } else {
                    op.pos = { el.bound.x + pH, el.bound.y, el.bound.w - pH * 2, el.bound.h };
                }
                m_textOps.emplace_back(op);
            }

            // Draw image (if present)
            if (!style.imageSrc.empty()) {
                const std::string & imageSrc = style.imageSrc;
                ImageOp             op;
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
                    op.tinted   = true;
                    op.tint     = theme.tabArrow.fg;
                    op.isFilled = true;
                } else if (el.type == UiElementType::TabClose) {
                    op.tinted = true;
                    op.tint   = style.colors.fg;
                }
                // A disabled element's icon is flat-tinted with the same colour
                // disabled menu text uses, so buttons and menu items read as one
                // system. Wins over any tint above - being unusable outranks
                // whatever the element normally looks like. The no-animation half
                // is already free: a Disabled element never becomes Hovered or
                // Active, so it gets neither the hover shadow nor the press scale.
                if (el.state == UiElementState::Disabled) {
                    op.tinted = true;
                    op.tint   = theme.menuItemDisabledColor;
                }
                op.hovered = (el.state == UiElementState::Hovered);
                op.active  = (el.state == UiElementState::Active);
                m_imageOps.emplace_back(op);
            }
        }

        appendToolbarTooltip(theme, m_resManager.buttons(), m_resManager.localeManager());
    }

    // Tooltip for whichever toolbar button is hovered. Emitted after every
    // element so it overlays them, and as raw ops rather than a UiElement -
    // a tooltip is decoration and must not be hit-testable.
    void appendToolbarTooltip(const Ui::Res::Type::theme_t &               theme,
                              const std::vector<Ui::Res::Type::button_t> & buttons,
                              const Ui::Res::LocaleManager &               localeMgr)
    {
        const Ui::font_handle_t font = m_layout.statusBarFont();
        if (font == 0) {
            return;
        }

        const UiElement * hovered = nullptr;
        for (const auto & el : m_layout.elements()) {
            if (el.type == UiElementType::ToolbarButton && el.state == UiElementState::Hovered) {
                hovered = &el;
                break;
            }
        }
        if (hovered == nullptr) {
            return;
        }

        std::string tip;
        bool        isRight = false;
        for (const auto & button : buttons) {
            if (button.id == hovered->id) {
                tip     = localeMgr.get(button.tooltip);
                isRight = (button.anchor == Ui::Res::Dock::DockAnchor::Right);
                break;
            }
        }
        if (tip.empty()) {
            return;
        }

        // Sized like a one-row popup, since it borrows the dropdown surface
        const Ui::Res::Type::popup_t & popup = m_resManager.popup();

        const std::wstring wide = Common::Unicode::fromUtf8(tip);
        const fpx_t        padH = popup.itemPaddingH;
        const fpx_t        tipW = m_render->textWidth(font, wide) + (padH * 2.0F);
        const fpx_t        tipH = popup.itemHeight;
        const fpx_t        tipY = hovered->bound.y + ((hovered->bound.h - tipH) / 2.0F);
        const fpx_t        tipX = isRight ? hovered->bound.x - tipW - padH : hovered->bound.x + hovered->bound.w + padH;

        addBgOp({ tipX, tipY, tipW, tipH }, theme.dropdown.bg);

        TextOp op;
        op.text  = tip;
        op.hFont = font;
        op.color = theme.dropdown.fg;
        op.pos   = { tipX + padH, tipY, tipW - (padH * 2.0F), tipH };
        m_textOps.emplace_back(op);
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
        case UiElementType::ToolbarButton:
            // Keep the normal background - the greyed icon carries the signal - but
            // match MenuButton's disabled foreground so a button that renders a
            // label instead of an icon greys too.
            if (el.state == UiElementState::Disabled) {
                return { theme.menuItemDisabledColor, theme.button.bg };
            }
            // Armed tool: shifted away from the button background, derived via
            // the same theme-aware helper edges use, so darkening does not
            // vanish on a dark theme and no theme needs a new key
            if (m_resManager.isActiveButton(el.id)) {
                return { theme.button.fg,
                         theme.button.bg.edgeColor(theme.buttonActiveContrast, m_resManager.isThemeDark()) };
            }
            return theme.button;
        case UiElementType::Text:
            if (el.state == UiElementState::Active) {
                return theme.statusBarActive;
            }
            return theme.statusBar;
        // Painted by their owner (DockColumn) or not at all, so this must paint
        // nothing: Ui::Color's default is debug magenta, which the bg gate below
        // would happily draw under every dock element
        default: return { Ui::Color::Inherit(), Ui::Color::Inherit() };
        }
    }

    // Colors stay in their own method: a TabClose bg needs its parent's alone, and
    // a full parent resolve would re-truncate that tab's label per close button
    [[nodiscard]] element_style_t resolveStyle(const UiElement & el, const Ui::Res::Type::theme_t & theme) const
    {
        const auto & layout    = m_resManager.layout();
        const auto & localeMgr = m_resManager.localeManager();

        element_style_t style;
        style.colors = resolveColors(el, theme);

        switch (el.type) {
        case UiElementType::MenuButton:
            style.font = m_layout.menuFont();
            style.padH = layout.menuButtonPadH;
            for (const auto & menu : m_resManager.menus()) {
                if (menu.id != el.id) {
                    continue;
                }
                // An icon menu draws the theme icon, not the icon under its own key
                if (menu.icon.empty()) {
                    style.text = localeMgr.get(menu.label);
                } else {
                    style.imageSrc = m_resManager.resPath().icon(m_resManager.themeIcon());
                }
                break;
            }
            return style;
        case UiElementType::ToolbarButton:
            for (const auto & button : m_resManager.buttons()) {
                if (button.id != el.id) {
                    continue;
                }
                style.text = localeMgr.get(button.label);
                if (!button.icon.empty()) {
                    // A bare name is a bundled icon; anything with a separator
                    // is a path the res data supplied outright
                    style.imageSrc = (button.icon.find('/') == std::string::npos)
                                   ? m_resManager.resPath().icon(button.icon)
                                   : button.icon;
                }
                break;
            }
            return style;
        case UiElementType::Tab: {
            style.padH            = layout.workspaceTab.padding;
            const Ui::tab_t * tab = m_resManager.tabBar().find(el.id);
            if (tab == nullptr) {
                return style;
            }
            style.font           = tab->isActive ? m_layout.itemFontBold() : m_layout.itemFont();
            const fpx_t textMaxW = el.bound.w - layout.workspaceTab.padding * 2 - layout.workspaceTab.height;
            style.text           = Ui::Render::truncateFileName(tab->label, textMaxW, m_render.get(), style.font);
            return style;
        }
        case UiElementType::TabClose: {
            const std::string & icon = m_resManager.tabCloseIcon();
            if (!icon.empty()) {
                style.imageSrc = m_resManager.resPath().icon(icon);
            }
            return style;
        }
        case UiElementType::TabArrow: {
            const std::string & icon = (el.id == TAB_ARROW_LEFT) ? m_resManager.tabArrowLeft()
                                                                 : m_resManager.tabArrowRight();
            style.imageSrc           = m_resManager.resPath().icon(icon);
            return style;
        }
        case UiElementType::Text:
            style.font = m_layout.statusBarFont();
            style.text = m_resManager.statusText();
            return style;
        case UiElementType::MenuItem:
        case UiElementType::Separator:
        case UiElementType::Image:
        case UiElementType::DockRow:
        case UiElementType::DockExpander:
        case UiElementType::DockGrip:
        case UiElementType::DockSlider:
        case UiElementType::DockScrollbar: return style;
        }
        return style;
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
            m_render->fillRect(op.bound, op.radius, op.colors, {});
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
            m_render->drawImage(op.src, op.pos, op.radius, tint, scale, shadow, op.isFilled);
            // Keep the pressed-size variant warm: the press effect draws the
            // icon at iconActiveScale (res JSON --button-icon-active-scale),
            // which is its own size-keyed texture-cache entry - warming here
            // means the first click never rasterizes mid-frame. A map hit once
            // cached.
            if (!op.active) {
                m_render->warmImage(op.src, op.pos, tint, m_resManager.layout().iconActiveScale, op.isFilled);
            }
        }

        for (const auto & op : m_textOps) {
            m_render->drawText(op.hFont, op.text, op.pos, op.color, op.alignH, op.alignV, op.minPadH);
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
            m_render->fillRect({ p.tabRect.x, p.tabRect.y, p.sectorW * totalSectors, p.tabRect.h },
                               p.tabRadius,
                               loadColors,
                               {});
            return;
        }

        fpx_t curX = p.tabRect.x;

        const Ui::Res::Type::border_t firstRadius = { p.tabRadius.topLeft, 0, 0, p.tabRadius.bottomLeft };
        m_render->fillRect({ curX, p.tabRect.y, p.sectorW, p.tabRect.h }, firstRadius, loadColors, {});
        curX += p.sectorW;

        const int midCount = p.progress - 1;
        if (midCount > 0) {
            const fpx_t midW = p.sectorW * midCount;
            m_render->fillRect({ curX, p.tabRect.y, midW, p.tabRect.h }, {}, loadColors, {});
            curX += midW;
        }

        // Arrow tip |>. Snap to the physical pixel grid so fractional tab
        // geometry does not leave the tip at an arbitrary sub-pixel position.
        const fpx_t tipX = std::round(curX + p.sectorW);
        const fpx_t tipY = std::round(p.tabRect.y + p.tabRect.h / 2.0F);
        m_render->drawTriangle(curX, p.tabRect.y, tipX, tipY, curX, p.tabRect.y + p.tabRect.h, loadColors.fg);
    }

    // Core members
    UiLayout                    m_layout;
    const Ui::Res::ResManager & m_resManager;

    // GL implementation of the draw sink (owns the font/svg/rounded backends).
    // mutable: text measurement (a logically-const query) populates the glyph
    // atlas cache, so const resolve* methods can measure through it.
    std::unique_ptr<Ui::IRender> m_render;

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
    std::function<void(UiLayout &)>   m_extraElementsHook;

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
