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

#include "common/noncopyable.h"
#include "common/time.h"
#include "common/unicode.h"
#include "ui/animation/animator.h"
#include "ui/animation/transition.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/const.h"
#include "ui/gl/svgrenderer.h"
#include "ui/idkind.h"
#include "ui/interface/irender.h"
#include "ui/interface/irenderer.h"
#include "ui/render/bgop.h"
#include "ui/render/drawlayer.h"
#include "ui/render/elementevent.h"
#include "ui/render/elementkey.h"
#include "ui/render/elementstyle.h"
#include "ui/render/eventkind.h"
#include "ui/render/imageop.h"
#include "ui/render/progressop.h"
#include "ui/render/shadow.h"
#include "ui/render/tabcloseboundof.h"
#include "ui/render/textop.h"
#include "ui/render/truncate.h"
#include "ui/render/uielement.h"
#include "ui/render/uielementstate.h"
#include "ui/render/uilayout.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/key/element.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/alignh.h"
#include "ui/res/type/alignv.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/button.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/menu.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/type.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
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
        // A resize is no move: tabs sized to the window jump with it, as they would chase it
        if (w != m_width || h != m_height) {
            m_animator = Ui::Animation::Animator<element_key_t> {};
        }
        // Cache the new physical size; the layout rebuilds on the next
        // setContent() using clientWidth/clientHeight.
        m_width  = w;
        m_height = h;
    }

    void cleanup() override { m_layout = UiLayout(); }

    void apply(Ui::Res::Type::changes_t changed) override
    {
        // A theme switch is no change of state: every colour lands on the new theme's at once,
        // as the grounds drawn around them do
        if (changed.has(Ui::Res::Type::Changed::Theme)) {
            m_animator = Ui::Animation::Animator<element_key_t> {};
        }
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
        const UiElement *                   el = m_layout.elementById(id);
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
        // buffers. The ops are then translated to IRender primitives, the held
        // tab's in a layer of their own over everything else.
        m_layer.clear();
        m_heldLayer.clear();

        generateDrawOps();
        if (m_extraOpsHook) {
            m_extraOpsHook(*this);
        }

        m_animator.sweep();

        m_render->beginFrame(width, height);
        flushOps(m_layer);
        if (!m_heldLayer.empty()) {
            m_render->flushLayer();
            flushOps(m_heldLayer);
        }
        m_render->endFrame();
    }

    // The colours a hook user's element `key` is drawn in this frame: `target`, the theme's for
    // its state, faded along `block`'s transition in animation.json, as the renderer's own are
    [[nodiscard]] Ui::Res::Type::color_pair_t colorsOf(const element_key_t &               key,
                                                       const Ui::Res::Type::color_pair_t & target,
                                                       Ui::Res::Key::ElementKey            block)
    {
        return m_animator.colorsOf(key, target, m_resManager.transitionsFor(block), m_frameTime);
    }

    // The tab `id` dragged, its left edge `left` CSS px: drawn there, held along the strip
    // and over every other tab; let go, it glides to where layout puts it
    void holdTab(id_t id, fpx_t left)
    {
        m_heldTabId   = id;
        m_heldTabLeft = left;
    }

    void releaseTab() { m_heldTabId = INVALID_ID; }

    // Whether the last frame drew something mid-glide, so another frame is due
    [[nodiscard]] bool isGliding() const { return m_animator.isRunning(m_frameTime); }

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
        bg_op_t op;
        op.bound     = rect;
        op.radius    = radius;
        op.colors.fg = elementColors.bg; // shape fill
        op.colors.bg = parentBg;         // AA blend color
        m_layer.bgOps.emplace_back(op);
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
        text_op_t op;
        op.text    = text;
        op.hFont   = font;
        op.color   = color;
        op.pos     = rect;
        op.alignH  = alignH;
        op.alignV  = alignV;
        op.minPadH = minPadH;
        m_layer.textOps.emplace_back(op);
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

    // Measured here rather than by the popup's own renderer, which is built after
    // its window - and the window needs the width first
    [[nodiscard]] fpx_t popupWidthOf(const std::vector<Ui::Res::Type::menu_t> & items) const
    {
        return m_layout.popupWidthOf(items, m_resManager, *m_render);
    }

    // The Ctrl+Tab list's rows fitted to `maxWidth` and the CSS width they take,
    // measured here for the same reason
    [[nodiscard]] fpx_t fitTabSwitcherRows(fpx_t maxWidth, std::vector<Ui::tab_t> & rows) const
    {
        return m_layout.fitTabSwitcherRows(maxWidth, m_resManager, *m_render, rows);
    }

    // A tooltip's CSS width, measured here for the same reason
    [[nodiscard]] fpx_t tooltipWidthOf(const std::string & text) const
    {
        return textWidth(m_layout.tooltipFont(), text) + (m_resManager.layout().tooltip.padding * 2.0F);
    }

    // The toolbar or menu-bar button the pointer is on, hovered or pressed - what may
    // carry a tooltip; nullptr over none
    [[nodiscard]] const UiElement * hoveredButton() const
    {
        for (const auto & el : m_layout.elements()) {
            if ((el.type == UiElementType::ToolbarButton || el.type == UiElementType::MenuButton)
                && (el.state == UiElementState::Hovered || el.state == UiElementState::Active)) {
                return &el;
            }
        }
        return nullptr;
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
        image_op_t op;
        op.src      = svgPath;
        op.pos      = rect;
        op.tint     = tint;
        op.tinted   = true;
        op.isSvg    = true;
        op.isFilled = isFilled;
        m_layer.imageOps.emplace_back(op);
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
                                 Ui::Window::key_modifiers_t /*modifiers*/) override
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
        m_mouseX    = Ui::NO_POINTER;
        m_mouseY    = Ui::NO_POINTER;
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
            const bool isHovered  = el.bound.contains(cssX, cssY) && el.accepts.has(EventKind::Hover);
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
     * Iterates the flat element list and produces bg_op_t/text_op_t/image_op_t directly.
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
        addBgOp({ .w = cssW, .h = layout.topMenu.height }, theme.topMenu.bg);
        // Left toolbar
        addBgOp({ .y = layout.topMenu.height, .w = layout.leftToolbar.width, .h = contentH }, theme.leftToolbar.bg);
        // Right toolbar
        addBgOp({ .x = cssW - layout.rightToolbar.width,
                  .y = layout.topMenu.height,
                  .w = layout.rightToolbar.width,
                  .h = contentH },
                theme.rightToolbar.bg);
        // Workspace tabs strip
        addBgOp({ .x = layout.leftToolbar.width,
                  .y = layout.topMenu.height,
                  .w = cssW - layout.leftToolbar.width - layout.rightToolbar.width,
                  .h = layout.workspaceTab.height },
                theme.workspaceTabs.bg);
        // Workspace area
        addBgOp({ .x = layout.leftToolbar.width,
                  .y = layout.topMenu.height + layout.workspaceTab.height,
                  .w = cssW - layout.leftToolbar.width - layout.rightToolbar.width,
                  .h = contentH - layout.workspaceTab.height },
                theme.workspace.bg);
        // Status bar
        addBgOp({ .y = cssH - layout.statusBar.height, .w = cssW, .h = layout.statusBar.height }, theme.statusBar.bg);

        // ---- Interactive elements ----
        const auto & tabBar = m_resManager.tabBar();
        // A scroll is no move: the strip shows other tabs, which jump into place
        if (tabBar.scrollOffset() != m_scrollOffset) {
            m_scrollOffset = tabBar.scrollOffset();
            m_animator     = Ui::Animation::Animator<element_key_t> {};
        }
        m_frameTime = Common::Time::now();
        // The tab drawn last, which its close glyph right after it stands in and on
        Ui::Res::Type::bound_t drawnTab;
        Ui::Color              drawnTabBg;

        for (const auto & el : m_layout.elements()) {
            const element_key_t                              key { .id = el.id, .type = el.type };
            const std::vector<Ui::Animation::transition_t> & transitions = transitionsFor(el.type);
            element_style_t                                  style       = resolveStyle(el, theme);
            style.colors                       = m_animator.colorsOf(key, style.colors, transitions, m_frameTime);
            const Ui::Res::Type::bound_t bound = drawnBoundOf(el, key, transitions, drawnTab);
            if (el.type == UiElementType::Tab) {
                drawnTab   = bound;
                drawnTabBg = style.colors.bg;
            }
            const bool     isHeld = m_heldTabId != INVALID_ID && el.id == m_heldTabId;
            draw_layer_t & layer  = isHeld ? m_heldLayer : m_layer;

            // Draw background (if visible and not inherited from parent)
            if (style.colors.bg.a() > 0 && !style.colors.bg.isInherit()) {
                bg_op_t op;
                op.bound     = bound;
                op.colors.fg = style.colors.bg;
                if (el.type == UiElementType::Tab) {
                    op.radius = layout.workspaceTab.border;
                    // Held, it stands over its neighbours: its corners show them, not the strip
                    op.colors.bg = isHeld ? Ui::Color::TransparentBlack() : theme.workspaceTabs.bg;
                } else if (el.type == UiElementType::TabClose) {
                    op.radius    = layout.tabCloseBorderRadius;
                    op.colors.bg = drawnTabBg;
                } else if (el.type == UiElementType::MenuButton) {
                    op.colors.bg = theme.topMenu.bg;
                    if (el.id == m_resManager.activeMenuId() || el.state == UiElementState::Active) {
                        op.radius = layout.menuButtonActiveBorder;
                    } else if (el.state == UiElementState::Hovered) {
                        op.radius = layout.menuButtonHoverBorder;
                    }
                } else if (el.type == UiElementType::ToolbarButton) {
                    // Rounded against the toolbar it belongs to (resolveStyle, its anchor)
                    op.radius    = layout.toolbarButton.border;
                    op.colors.bg = style.backdrop;
                }
                layer.bgOps.emplace_back(op);
            }

            // Draw loading progress bar overlay on workspace tabs
            int modelProgress = 0;
            if (el.type == UiElementType::Tab) {
                tabBar.visit(el.id, [&modelProgress](const Ui::tab_t & tab) {
                    modelProgress = tab.isLoading ? tab.progress : 0;
                });
            }
            if (modelProgress > 0) {
                const int   tlr          = roundToInt(layout.workspaceTab.border.topLeft);
                const int   totalSectors = (tlr > 0) ? std::max(2, static_cast<int>(bound.w) / tlr)
                                                     : layout.progressSectors;
                const fpx_t sectorW      = bound.w / totalSectors;
                // Scale model progress (0..layout.progressSectors) to visual sectors
                const int progress = std::max(1, modelProgress * totalSectors / layout.progressSectors);
                layer.progressOps.emplace_back(progress_op_t { .tabRect      = bound,
                                                               .tabRadius    = layout.workspaceTab.border,
                                                               .sectorW      = sectorW,
                                                               .totalSectors = totalSectors,
                                                               .progress     = progress });
            }

            // Draw text (if present)
            if (!style.text.empty() && style.font != 0) {
                const fpx_t pH = style.padH;
                text_op_t   op;
                op.text  = std::move(style.text);
                op.hFont = style.font;
                op.color = style.colors.fg;

                // A menu button and a tab centre while their label fits and
                // fall back to left-aligned when it does not, so a long file
                // name does not spill out of the left edge
                const bool isCentered = (el.type == UiElementType::MenuButton || el.type == UiElementType::Tab);
                op.alignH             = isCentered ? Ui::Res::Type::AlignH::CenterClamped : Ui::Res::Type::AlignH::Left;
                if (isCentered) {
                    op.pos     = bound;
                    op.minPadH = pH;
                } else {
                    op.pos = { .x = bound.x + pH, .y = bound.y, .w = bound.w - (pH * 2), .h = bound.h };
                }
                layer.textOps.emplace_back(op);
            }

            // Draw image (if present)
            if (!style.imageSrc.empty()) {
                const std::string & imageSrc = style.imageSrc;
                image_op_t          op;
                op.src   = imageSrc;
                op.isSvg = isSvgFile(imageSrc);
                // A toolbar button's icon is the button inset by its padding
                const fpx_t buttonIcon = std::min(bound.w, bound.h) - (layout.toolbarButton.padding * 2.0F);
                const fpx_t imgSz      = [&]() -> fpx_t {
                    switch (el.type) {
                    case UiElementType::TabClose: return layout.tabCloseIconSize;
                    case UiElementType::TabArrow: return bound.h;
                    case UiElementType::ToolbarButton: return buttonIcon;
                    default: return layout.buttonImgSize;
                    }
                }();
                fpx_t imgW = imgSz;
                fpx_t imgH = imgSz;
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
                const fpx_t imgX = bound.x + ((bound.w - imgW) / 2.0F);
                const fpx_t imgY = bound.y + ((bound.h - imgH) / 2.0F);
                op.pos           = { .x = imgX, .y = imgY, .w = imgW, .h = imgH };
                if (el.type == UiElementType::TabArrow) {
                    op.tinted   = true;
                    op.tint     = theme.tabArrow.fg;
                    op.isFilled = true;
                } else if (el.type == UiElementType::TabClose) {
                    op.tinted = true;
                    op.tint   = style.colors.fg;
                }
                // A disabled element's icon is flat-tinted: a toolbar button's with
                // its own (toolbar-button:disabled), anything else's with the colour
                // disabled menu text uses. Wins over any tint above - being unusable
                // outranks whatever the element normally looks like. The
                // no-animation half is already free: a Disabled element never
                // becomes Hovered or Active, so it gets neither the hover shadow nor
                // the press scale.
                if (el.state == UiElementState::Disabled) {
                    op.tinted = true;
                    op.tint   = (el.type == UiElementType::ToolbarButton) ? theme.toolbarButtonDisabledColor
                                                                          : theme.menuItemDisabledColor;
                }
                op.hovered = (el.state == UiElementState::Hovered);
                op.active  = (el.state == UiElementState::Active);
                layer.imageOps.emplace_back(op);
            }
        }
    }

    // Where `el` is drawn this frame: where its glide along `transitions` stands, a held tab
    // where the drag puts it along the strip, a close glyph in `drawnTab`, the tab drawn right
    // before it
    [[nodiscard]] Ui::Res::Type::bound_t drawnBoundOf(const UiElement &                                el,
                                                      const element_key_t &                            key,
                                                      const std::vector<Ui::Animation::transition_t> & transitions,
                                                      const Ui::Res::Type::bound_t &                   drawnTab)
    {
        if (el.type == UiElementType::TabClose) {
            return Ui::Render::tabCloseBoundOf(m_resManager.layout(), drawnTab);
        }
        if (el.type == UiElementType::Tab && m_heldTabId != INVALID_ID && el.id == m_heldTabId) {
            Ui::Res::Type::bound_t held = el.bound;
            held.x                      = heldTabLeftOf(el.bound.w);
            m_animator.hold(key, held);
            return held;
        }
        return m_animator.boundOf(key, el.bound, transitions, m_frameTime);
    }

    // An element's transitions, its block's in animation.json; none for one with no block there
    [[nodiscard]] const std::vector<Ui::Animation::transition_t> & transitionsFor(UiElementType type) const
    {
        static const std::vector<Ui::Animation::transition_t> none;
        switch (type) {
        case UiElementType::Tab: return m_resManager.transitionsFor(Ui::Res::Key::ElementKey::WorkspaceTab);
        case UiElementType::TabClose: return m_resManager.transitionsFor(Ui::Res::Key::ElementKey::WorkspaceTabClose);
        case UiElementType::ToolbarButton: return m_resManager.transitionsFor(Ui::Res::Key::ElementKey::ToolbarButton);
        case UiElementType::MenuButton:
            return m_resManager.transitionsFor(Ui::Res::Key::ElementKey::TopMenuButtonLabel);
        default: return none;
        }
    }

    // The held tab's left edge, kept from the strip's first tab's left to its last tab's right
    [[nodiscard]] fpx_t heldTabLeftOf(fpx_t width) const
    {
        fpx_t first = std::numeric_limits<fpx_t>::max();
        fpx_t last  = std::numeric_limits<fpx_t>::lowest();
        for (const auto & el : m_layout.elements()) {
            if (el.type == UiElementType::Tab) {
                first = std::min(first, el.bound.x);
                last  = std::max(last, el.bound.x + el.bound.w);
            }
        }
        return std::max(first, std::min(m_heldTabLeft, last - width));
    }

    // Resolve color pair for an element from ResManager theme
    [[nodiscard]] Ui::Res::Type::color_pair_t resolveColors(const UiElement &              el,
                                                            const Ui::Res::Type::theme_t & theme) const
    {
        switch (el.type) {
        case UiElementType::MenuButton: {
            if (el.state == UiElementState::Disabled) {
                return { .fg = theme.menuItemDisabledColor, .bg = theme.topMenuButton.bg };
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
            const bool isActiveTab = el.id == m_resManager.tabBar().activeId();
            if (el.state == UiElementState::Active) {
                return theme.workspaceTabActive;
            }
            if (el.state == UiElementState::Hovered) {
                return theme.workspaceTabHover;
            }
            if (isActiveTab) {
                return { .fg = theme.workspaceTab.fg, .bg = theme.workspaceTabActive.bg };
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
            // Disabled - no action behind it - it keeps its normal background: its
            // icon, flat-tinted (toolbar-button:disabled), carries the signal
            if (el.state == UiElementState::Disabled) {
                return theme.toolbarButton;
            }
            // Armed - the tool it stands for is on - as a menu item's active is the
            // current value; hovered and pressed alike otherwise
            if (m_resManager.isActiveButton(el.id)) {
                return theme.toolbarButtonActive;
            }
            if (el.state == UiElementState::Hovered || el.state == UiElementState::Active) {
                return theme.toolbarButtonHover;
            }
            return theme.toolbarButton;
        case UiElementType::Text:
            if (el.state == UiElementState::Active) {
                return theme.statusBarActive;
            }
            return theme.statusBar;
        // Painted by their owner (DockColumn) or not at all, so this must paint
        // nothing: Ui::Color's default is debug magenta, which the bg gate below
        // would happily draw under every dock element
        default: return { .fg = Ui::Color::Inherit(), .bg = Ui::Color::Inherit() };
        }
    }

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
            if (const Ui::Res::Type::button_t * button = m_resManager.findButton(el.id)) {
                style.text     = localeMgr.get(button->label);
                style.backdrop = (button->anchor == Ui::Res::Dock::DockAnchor::Right) ? theme.rightToolbar.bg
                                                                                      : theme.leftToolbar.bg;
                if (!button->icon.empty()) {
                    style.imageSrc = m_resManager.resPath().iconPathOf(button->icon);
                }
            }
            return style;
        case UiElementType::Tab: {
            style.padH = layout.workspaceTab.padding;
            m_resManager.tabBar().visit(el.id, [&](const Ui::tab_t & tab) {
                style.font           = tab.isActive ? m_layout.itemFontBold() : m_layout.itemFont();
                const fpx_t textMaxW = el.bound.w - (layout.workspaceTab.padding * 2) - layout.workspaceTab.height;
                style.text           = Ui::Render::truncateFileName(tab.label, textMaxW, m_render.get(), style.font);
            });
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
        case UiElementType::DockScrollbar:
        case UiElementType::DockMenu: return style;
        }
        return style;
    }

    // Add a simple background op (for region backgrounds)
    void addBgOp(const Ui::Res::Type::bound_t & bound, const Ui::Color & color)
    {
        if (color.a() > 0) {
            bg_op_t op;
            op.bound     = bound;
            op.colors.fg = color;
            m_layer.bgOps.emplace_back(op);
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

    // Translate one layer's staged ops into IRender primitive calls. The bits
    // that read theme/layout (icon scale/shadow, progress colors) are resolved
    // here and passed as plain values, so the backend stays domain-blind. Order
    // (backgrounds -> progress -> images -> text) preserves the layering.
    void flushOps(const draw_layer_t & layer)
    {
        for (const auto & op : layer.bgOps) {
            m_render->fillRect(op.bound, op.radius, op.colors, {});
        }

        for (const auto & p : layer.progressOps) {
            drawProgress(p);
        }

        for (const auto & op : layer.imageOps) {
            const fpx_t          scale = op.active ? m_resManager.layout().iconActiveScale : 1.0F;
            Ui::Render::shadow_t shadow {};
            if (op.hovered) {
                const auto & layout = m_resManager.layout();
                const auto & theme  = m_resManager.theme();
                shadow              = { .offsetX = layout.iconHoverShadowX,
                                        .offsetY = layout.iconHoverShadowY,
                                        .blur    = layout.iconHoverShadowBlur,
                                        .opacity = theme.shadowOpacity,
                                        .color   = theme.colorShadow };
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

        for (const auto & op : layer.textOps) {
            m_render->drawText(op.hFont, op.text, op.pos, op.color, op.alignH, op.alignV, op.minPadH);
        }
    }

    // Compose the tab loading bar from rounded-rect sectors plus the flat arrow
    // tip, all through IRender primitives. progress N (1..total): first sector
    // rounded (TL/BL), middle sectors plain, then the |> tip; at total, a fully
    // rounded bar.
    void drawProgress(const progress_op_t & p)
    {
        const int totalSectors = p.totalSectors;
        if (p.sectorW <= 0 || p.tabRect.h <= 0 || p.progress <= 0) {
            return;
        }

        const Ui::Res::Type::color_pair_t loadColors = { .fg = m_resManager.theme().colorLoad,
                                                         .bg = m_resManager.theme().workspaceTabs.bg };

        if (p.progress >= totalSectors) {
            m_render->fillRect({ .x = p.tabRect.x, .y = p.tabRect.y, .w = p.sectorW * totalSectors, .h = p.tabRect.h },
                               p.tabRadius,
                               loadColors,
                               {});
            return;
        }

        fpx_t curX = p.tabRect.x;

        const Ui::Res::Type::border_t firstRadius = { .topLeft    = p.tabRadius.topLeft,
                                                      .bottomLeft = p.tabRadius.bottomLeft };
        m_render->fillRect({ .x = curX, .y = p.tabRect.y, .w = p.sectorW, .h = p.tabRect.h },
                           firstRadius,
                           loadColors,
                           {});
        curX += p.sectorW;

        const int midCount = p.progress - 1;
        if (midCount > 0) {
            const fpx_t midW = p.sectorW * midCount;
            m_render->fillRect({ .x = curX, .y = p.tabRect.y, .w = midW, .h = p.tabRect.h }, {}, loadColors, {});
            curX += midW;
        }

        // Arrow tip |>. Snap to the physical pixel grid so fractional tab
        // geometry does not leave the tip at an arbitrary sub-pixel position.
        const fpx_t tipX = std::round(curX + p.sectorW);
        const fpx_t tipY = std::round(p.tabRect.y + (p.tabRect.h / 2.0F));
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

    // Batched draw ops: the frame's, and over them the held tab's
    draw_layer_t m_layer;
    draw_layer_t m_heldLayer;

    // Where each element is drawn, and in which colours, while it glides; layout and the theme
    // stay the truth hit-testing and every other reader take
    Ui::Animation::Animator<element_key_t> m_animator;
    Common::Time::point_t                  m_frameTime;        // when the last frame was drawn
    std::size_t                            m_scrollOffset = 0; // the strip's when last drawn, a change of it jumping
    id_t                                   m_heldTabId    = INVALID_ID; // the dragged tab (holdTab)
    fpx_t                                  m_heldTabLeft  = 0.0F;       // its left edge as the drag puts it, CSS px

    // Extra-ops hook fires after generateDrawOps() and before the ops are
    // flushed, so callers can append BG/Image ops into the same frame (docks).
    std::function<void(UiRenderer &)> m_extraOpsHook;
    std::function<void(UiLayout &)>   m_extraElementsHook;

    // Mouse state
    fpx_t m_mouseX        = Ui::NO_POINTER;
    fpx_t m_mouseY        = Ui::NO_POINTER;
    bool  m_mouseDown     = false;
    fpx_t m_lastMouseCssX = -1;
    fpx_t m_lastMouseCssY = -1;

    // Callbacks
    std::function<void(UiElementType, id_t)> m_onElementHover;
    id_t                                     m_lastHoverId = INVALID_ID;
};

} // namespace Ui::Render
