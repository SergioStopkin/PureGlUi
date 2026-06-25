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
#include "ui/gl/textalign.h"
#include "ui/render/popup/popuprendererbase.h"
#include "ui/res/type/dialog.h"
#include "ui/type.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace Ui::Render::Popup {

struct alignas(64) dialog_button_t final {
    Ui::Res::Type::bound_t      bound {};
    std::string                 label;
    Ui::Render::UiElementState  state   = Ui::Render::UiElementState::None;
    bool                        primary = false;
    Ui::Res::Type::DialogAction action  = Ui::Res::Type::DialogAction::None;

    bool operator==(const dialog_button_t &) const = default;
};

/**
 * @brief Renderer for modal dialog windows
 *
 * Renders a centered dialog with:
 * - Title bar with optional icon and close button (X)
 * - Content text area
 * - Button row at the bottom
 */
class DialogRenderer final : public PopupRendererBase {
public:
    using DialogCloseFn = std::function<void(Ui::Res::Type::DialogAction)>;

    DialogRenderer(Ui::IWindow & window, const Ui::Res::ResManager & resManager, const Ui::Res::Type::dialog_t & dialog)
        : PopupRendererBase(window, resManager)
        , m_dialog(dialog)
    {
        if (m_uiRender->fontRenderer() != nullptr) {
            m_font      = m_uiRender->fontRenderer()->createFont(m_resManager.theme().dialogFont);
            m_titleFont = m_uiRender->fontRenderer()->createFont(m_resManager.theme().dialogTitleFont);
        }

        // Cache SVG icon keys
        if (!m_dialog.icon.empty()) {
            const std::string iconPath = m_resManager.resPath().icon(m_dialog.icon);
            m_iconKey                  = Ui::Gl::SvgRenderer::ensureLoaded(iconPath);
        }
        const std::string closeIconPath = m_resManager.resPath().icon(m_resManager.layout().dialogCloseIcon);
        m_closeIconKey                  = Ui::Gl::SvgRenderer::ensureLoaded(closeIconPath);

        buildButtons();
    }

    ~DialogRenderer() override { cleanup(); }

    DialogRenderer(const DialogRenderer &)             = delete;
    DialogRenderer(DialogRenderer &&)                  = delete;
    DialogRenderer & operator=(const DialogRenderer &) = delete;
    DialogRenderer & operator=(DialogRenderer &&)      = delete;

    using RenderRequestFn = std::function<void()>;

    void setCloseCallback(DialogCloseFn fn) { m_onClose = std::move(fn); }
    void setRenderRequest(RenderRequestFn fn) { m_renderRequest = std::move(fn); }

    void confirmPrimary()
    {
        for (const auto & btn : m_buttons) {
            if (btn.primary && m_onClose) {
                m_onClose(btn.action);
                return;
            }
        }
    }

    void dismiss()
    {
        if (m_onClose) {
            m_onClose(Ui::Res::Type::DialogAction::Ok);
        }
    }

    // Keyboard navigation: ESC, Enter, Left, Right
    void onKeyPress(const std::string & key)
    {
        if (m_keyAnimationTarget != KeyAnimationTarget::None) {
            return;
        }

        if (key == "Escape") {
            // Animate X close button: hover -> active -> dismiss
            clearKeyboardFocus();
            m_closeState         = Ui::Render::UiElementState::Hovered;
            m_keyAnimationTarget = KeyAnimationTarget::Close;
            startKeyAnimation();
        } else if (key == "Return") {
            // Animate focused button (or primary if none focused): hover -> active -> confirm
            size_t targetIndex = Ui::INVALID_ID;
            if (m_keyboardFocus != Ui::INVALID_ID && m_keyboardFocus < m_buttons.size()) {
                targetIndex = m_keyboardFocus;
            } else {
                for (size_t i = 0; i < m_buttons.size(); ++i) {
                    if (m_buttons[i].primary) {
                        targetIndex = i;
                        break;
                    }
                }
            }
            if (targetIndex != Ui::INVALID_ID) {
                clearKeyboardFocus();
                m_buttons[targetIndex].state = Ui::Render::UiElementState::Hovered;
                m_keyAnimationTarget         = KeyAnimationTarget::Button;
                m_keyAnimationIndex          = targetIndex;
                startKeyAnimation();
            }
        } else if (key == "Left") {
            moveKeyboardFocus(-1);
        } else if (key == "Right") {
            moveKeyboardFocus(1);
        }
    }

    void resize(fpx_t width, fpx_t height) override
    {
        if (m_width != width || m_height != height) {
            PopupRendererBase::resize(width, height);
            layoutButtons();
            layoutCloseButton();
        }
    }

    bool render() override
    {
        updateSmoothScroll();
        updateKeyAnimation();
        const bool premultiplied = beginRender();
        renderDialog(premultiplied);
        return true;
    }

    void cleanup() override
    {
        m_buttons.clear();
        PopupRendererBase::cleanup();
    }

    // === Ui::IEventApp Interface ===
    bool onMouseMove(int x, int y) override
    {
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);

        // Scrollbar thumb drag
        if (m_draggingThumb) {
            const fpx_t deltaY = cssY - m_dragStartY;
            const fpx_t thumbH = m_thumbBound.h;
            const fpx_t travel = m_trackBound.h - thumbH;
            if (travel > 0) {
                const fpx_t maxScroll = m_contentHeight - m_viewportHeight;
                const fpx_t value     = std::clamp(m_dragStartScroll + deltaY / travel * maxScroll, 0.0F, maxScroll);
                m_scrollOffset        = value;
                m_scrollTarget        = value;
            }
            return true;
        }

        bool changed = false;
        for (auto & btn : m_buttons) {
            const bool wasHovered = (btn.state == Ui::Render::UiElementState::Hovered);
            const bool isHovered  = btn.bound.contains(cssX, cssY);
            btn.state             = isHovered ? Ui::Render::UiElementState::Hovered : Ui::Render::UiElementState::None;
            if (isHovered != wasHovered) {
                changed = true;
            }
        }

        const bool wasCloseHovered = (m_closeState == Ui::Render::UiElementState::Hovered);
        const bool isCloseHovered  = m_closeBound.contains(cssX, cssY);
        m_closeState = isCloseHovered ? Ui::Render::UiElementState::Hovered : Ui::Render::UiElementState::None;
        if (isCloseHovered != wasCloseHovered) {
            changed = true;
        }

        if (m_contentHeight > m_viewportHeight) {
            const bool wasScrollbarHovered = m_scrollbarHovered;
            const bool wasThumbHovered     = m_thumbHovered;
            m_scrollbarHovered             = m_trackBound.contains(cssX, cssY);
            m_thumbHovered                 = m_thumbBound.contains(cssX, cssY);
            if (m_scrollbarHovered != wasScrollbarHovered || m_thumbHovered != wasThumbHovered) {
                std::cout << "[Dialog] scrollbar hover=" << m_scrollbarHovered << " thumb hover=" << m_thumbHovered
                          << " offset=" << m_scrollOffset << " target=" << m_scrollTarget << std::endl;
                changed = true;
            }
        }

        return changed;
    }

    bool onMouseLeave() override
    {
        for (auto & btn : m_buttons) {
            btn.state = Ui::Render::UiElementState::None;
        }
        m_closeState       = Ui::Render::UiElementState::None;
        m_scrollbarHovered = false;
        m_thumbHovered     = false;
        m_draggingThumb    = false;
        return true;
    }

    bool onScroll(int /*x*/, int /*y*/, fpx_t deltaY) override
    {
        if (m_contentHeight <= m_viewportHeight) {
            return false;
        }
        const fpx_t maxScroll = m_contentHeight - m_viewportHeight;
        const fpx_t direction = m_resManager.input().scrollNatural ? 1.0F : -1.0F;
        m_scrollTarget += deltaY * direction * m_resManager.input().scrollSpeed;
        m_scrollTarget = std::clamp(m_scrollTarget, 0.0F, maxScroll);
        std::cout << "[Dialog] onScroll deltaY=" << deltaY << " target=" << m_scrollTarget
                  << " offset=" << m_scrollOffset << " maxScroll=" << maxScroll << std::endl;
        return true;
    }

    bool onMousePress(int x, int y, int /*clickCount*/ = 1) override
    {
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);

        // Scrollbar: thumb grab or track click
        if (m_contentHeight > m_viewportHeight) {
            if (m_thumbBound.contains(cssX, cssY)) {
                m_draggingThumb   = true;
                m_dragStartY      = cssY;
                m_dragStartScroll = m_scrollOffset;
                return true;
            }
            if (m_trackBound.contains(cssX, cssY)) {
                const fpx_t maxScroll = m_contentHeight - m_viewportHeight;
                if (cssY < m_thumbBound.y) {
                    m_scrollTarget = std::clamp(m_scrollTarget - m_viewportHeight, 0.0F, maxScroll);
                } else {
                    m_scrollTarget = std::clamp(m_scrollTarget + m_viewportHeight, 0.0F, maxScroll);
                }
                return true;
            }
        }

        bool wasHit = false;

        if (m_closeBound.contains(cssX, cssY)) {
            m_closeState = Ui::Render::UiElementState::Active;
            wasHit       = true;
        } else {
            m_closeState = Ui::Render::UiElementState::None;
        }

        for (auto & btn : m_buttons) {
            if (!wasHit && btn.bound.contains(cssX, cssY)) {
                btn.state = Ui::Render::UiElementState::Active;
                wasHit    = true;
            } else if (btn.state == Ui::Render::UiElementState::Active) {
                btn.state = Ui::Render::UiElementState::None;
            }
        }
        return wasHit;
    }

    Ui::Render::click_result_t onMouseRelease(int x, int y) override
    {
        if (m_draggingThumb) {
            m_draggingThumb = false;
            return { .changed = true };
        }

        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);

        if (m_closeState == Ui::Render::UiElementState::Active && m_closeBound.contains(cssX, cssY)) {
            m_closeState = Ui::Render::UiElementState::None;
            dismiss();
            return { .changed = true };
        }
        m_closeState = Ui::Render::UiElementState::None;

        for (auto & btn : m_buttons) {
            if (btn.state == Ui::Render::UiElementState::Active && btn.bound.contains(cssX, cssY)) {
                btn.state = Ui::Render::UiElementState::None;
                if (m_onClose) {
                    m_onClose(btn.action);
                }
                return { .changed = true };
            }
            if (btn.state == Ui::Render::UiElementState::Active) {
                btn.state = Ui::Render::UiElementState::None;
            }
        }
        return {};
    }

private:
    enum class KeyAnimationTarget : uint8_t { None, Close, Button };

    void startKeyAnimation()
    {
        m_keyAnimationStart = std::chrono::steady_clock::now();
        m_keyAnimationPhase = 0;
        if (m_renderRequest) {
            m_renderRequest();
        }
    }

    void updateKeyAnimation()
    {
        if (m_keyAnimationTarget == KeyAnimationTarget::None) {
            return;
        }

        const auto  now     = std::chrono::steady_clock::now();
        const fpx_t elapsed = std::chrono::duration<fpx_t>(now - m_keyAnimationStart).count();

        const fpx_t delay = m_resManager.input().keyAnimationDelay;
        if (m_keyAnimationPhase == 0 && elapsed >= delay) {
            // Phase 1: hover -> active
            m_keyAnimationPhase = 1;
            if (m_keyAnimationTarget == KeyAnimationTarget::Close) {
                m_closeState = Ui::Render::UiElementState::Active;
            } else if (m_keyAnimationTarget == KeyAnimationTarget::Button && m_keyAnimationIndex < m_buttons.size()) {
                m_buttons[m_keyAnimationIndex].state = Ui::Render::UiElementState::Active;
            }
        }

        if (m_keyAnimationPhase == 1 && elapsed >= delay * 2) {
            // Phase 2: fire action
            m_keyAnimationPhase = 0;
            if (m_keyAnimationTarget == KeyAnimationTarget::Close) {
                m_closeState = Ui::Render::UiElementState::None;
                dismiss();
            } else if (m_keyAnimationTarget == KeyAnimationTarget::Button && m_keyAnimationIndex < m_buttons.size()) {
                m_buttons[m_keyAnimationIndex].state = Ui::Render::UiElementState::None;
                if (m_onClose) {
                    m_onClose(m_buttons[m_keyAnimationIndex].action);
                }
            }
            m_keyAnimationTarget = KeyAnimationTarget::None;
        }

        // Keep requesting frames while animating
        if (m_keyAnimationTarget != KeyAnimationTarget::None && m_renderRequest) {
            m_renderRequest();
        }
    }

    void clearKeyboardFocus()
    {
        for (auto & btn : m_buttons) {
            btn.state = Ui::Render::UiElementState::None;
        }
        m_closeState    = Ui::Render::UiElementState::None;
        m_keyboardFocus = Ui::INVALID_ID;
    }

    void moveKeyboardFocus(int direction)
    {
        if (m_buttons.empty()) {
            return;
        }

        const size_t count = m_buttons.size();

        // Clear previous hover state
        if (m_keyboardFocus != Ui::INVALID_ID && m_keyboardFocus < count) {
            m_buttons[m_keyboardFocus].state = Ui::Render::UiElementState::None;
        }

        if (m_keyboardFocus == Ui::INVALID_ID) {
            m_keyboardFocus = (direction > 0) ? 0 : count - 1;
        } else {
            int next = static_cast<int>(m_keyboardFocus) + direction;
            if (next < 0) {
                next = static_cast<int>(count) - 1;
            }
            m_keyboardFocus = static_cast<size_t>(next) % count;
        }

        m_buttons[m_keyboardFocus].state = Ui::Render::UiElementState::Hovered;

        if (m_renderRequest) {
            m_renderRequest();
        }
    }

    void updateSmoothScroll()
    {
        const auto now = std::chrono::steady_clock::now();
        if (m_lastFrameTime.time_since_epoch().count() == 0) {
            m_lastFrameTime = now;
            m_scrollOffset  = m_scrollTarget;
            return;
        }

        const fpx_t dt  = std::chrono::duration<fpx_t>(now - m_lastFrameTime).count();
        m_lastFrameTime = now;

        const fpx_t diff = m_scrollTarget - m_scrollOffset;
        if (std::abs(diff) < m_resManager.input().scrollSnapThreshold) {
            m_scrollOffset = m_scrollTarget;
            return;
        }

        const fpx_t smooth = m_resManager.input().scrollSmooth;
        const fpx_t factor = 1.0F - std::exp(-smooth * dt);
        m_scrollOffset += diff * factor;
        std::cout << "[Dialog] lerp offset=" << m_scrollOffset << " target=" << m_scrollTarget << " diff=" << diff
                  << " dt=" << dt << std::endl;

        // Request another frame while animating
        if (m_renderRequest) {
            m_renderRequest();
        }
    }

    void buildButtons()
    {
        m_buttons.clear();

        const auto & config = m_resManager.dialogTypeConfig(m_dialog.type);
        for (const auto & buttonConfig : config.buttons) {
            dialog_button_t btn;
            btn.label   = buttonConfig.label;
            btn.primary = buttonConfig.primary;
            btn.action  = buttonConfig.action;
            m_buttons.emplace_back(btn);
        }

        layoutButtons();
        layoutCloseButton();
    }

    void layoutButtons()
    {
        const auto & layout     = m_resManager.layout();
        const fpx_t  dialogCssW = m_width > 0 ? toCss(m_width) : layout.dialog.width;
        const fpx_t  dialogCssH = m_height > 0 ? toCss(m_height) : layout.dialog.height;
        const fpx_t  btnY       = dialogCssH - layout.dialog.padding - layout.dialogButtonH;

        // Measure total width of all buttons + gaps
        fpx_t totalW = 0;
        for (auto & btn : m_buttons) {
            fpx_t textW = layout.dialogButtonMinW;
            if (m_uiRender && m_uiRender->fontRenderer() != nullptr && m_font != 0) {
                const std::string & text = m_resManager.localeManager().get(btn.label);
                auto                tw   = m_uiRender->fontRenderer()->textWidth(m_font, text);
                textW                    = std::max(layout.dialogButtonMinW, tw + layout.dialogButtonPad * 2);
            }
            btn.bound = { 0, btnY, textW, layout.dialogButtonH };
            totalW += textW;
        }
        totalW += layout.dialogButtonPad * static_cast<fpx_t>(m_buttons.size() - 1);

        // Center the button row
        fpx_t cursorX = (dialogCssW - totalW) / 2.0F;
        for (auto & btn : m_buttons) {
            btn.bound.x = cursorX;
            cursorX += btn.bound.w + layout.dialogButtonPad;
        }
    }

    void layoutCloseButton()
    {
        const auto & layout     = m_resManager.layout();
        const fpx_t  dialogCssW = m_width > 0 ? toCss(m_width) : layout.dialog.width;
        const fpx_t  margin     = layout.dialogCloseMargin;
        const fpx_t  totalSize  = layout.dialogCloseSize + margin * 2;
        m_closeBound            = { dialogCssW - layout.dialogCloseRight - totalSize,
                                    layout.dialogCloseTop,
                                    totalSize,
                                    totalSize };
    }

    void renderDialog(bool premultiplied)
    {
        if (!m_uiRender || m_window == nullptr) {
            return;
        }

        const auto & theme = m_resManager.theme();
        const fpx_t  cssW  = toCss(m_width);
        const fpx_t  cssH  = toCss(m_height);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);

        const Ui::Color bgColor = premultiplied ? Ui::Color::TransparentBlack() : theme.dialog.bg;

        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_rounded.begin(m_width, m_height, g_config.scale);
        m_rounded.draw({ 0, 0, cssW, cssH }, m_resManager.layout().dialog.border, { theme.dialog.bg, bgColor });

        for (const auto & btn : m_buttons) {
            Ui::Color                    btnBg;
            const bool                   active   = (btn.state == Ui::Render::UiElementState::Active);
            const Ui::Res::Type::bound_t btnBound = active
                                                  ? shiftedBound(btn.bound, m_resManager.layout().dialogButtonShift)
                                                  : btn.bound;
            if (active) {
                btnBg = theme.dialogButtonActive.bg;
            } else if (btn.state == Ui::Render::UiElementState::Hovered) {
                btnBg = theme.dialogButtonHover.bg;
            } else if (btn.primary) {
                btnBg = theme.dialogButtonPrimary.bg;
            } else {
                btnBg = theme.dialogButton.bg;
            }
            m_rounded.draw(btnBound, m_resManager.layout().dialogButtonBorder, { btnBg, bgColor });
        }

        // Close button (X) background - skip when inherited (already drawn by dialog bg)
        {
            const Ui::Color rawBg = (m_closeState == Ui::Render::UiElementState::Active)  ? theme.dialogCloseActive.bg
                                  : (m_closeState == Ui::Render::UiElementState::Hovered) ? theme.dialogCloseHover.bg
                                                                                          : theme.dialogClose.bg;
            if (!rawBg.isInherit()) {
                m_rounded.draw(m_closeBound, m_resManager.layout().dialogCloseBorder, { rawBg, theme.dialog.bg });
            }
        }

        Ui::Gl::Rounded::end();

        auto * fontRenderer = m_uiRender->fontRenderer();
        if (fontRenderer == nullptr) {
            return;
        }

        // SVG icons (fixed-function pipeline)
        beginSvgDraw();

        if (!m_iconKey.empty() && Ui::Gl::SvgRenderer::isLoaded(m_iconKey)) {
            const auto &      iconBound = m_resManager.layout().dialogIcon;
            const Ui::Color & iconColor = (m_dialog.type == Ui::Res::Type::DialogType::Warning) ? theme.colorWarn
                                                                                                : theme.colorInfo;
            m_svgRenderer.drawTinted(m_iconKey,
                                     { iconBound.x * g_config.scale,
                                       iconBound.y * g_config.scale,
                                       iconBound.w * g_config.scale,
                                       iconBound.h * g_config.scale },
                                     iconColor);
        }

        if (!m_closeIconKey.empty() && Ui::Gl::SvgRenderer::isLoaded(m_closeIconKey)) {
            const fpx_t margin = m_resManager.layout().dialogCloseMargin;
            const fpx_t closeX = (m_closeBound.x + margin) * g_config.scale;
            const fpx_t closeY = (m_closeBound.y + margin) * g_config.scale;
            const fpx_t closeS = m_resManager.layout().dialogCloseSize * g_config.scale;

            const Ui::Color closeColor = (m_closeState == Ui::Render::UiElementState::Active)
                                       ? theme.dialogCloseActive.fg
                                       : (m_closeState == Ui::Render::UiElementState::Hovered)
                                       ? theme.dialogCloseHover.fg
                                       : theme.dialogClose.fg;
            m_svgRenderer.drawTinted(m_closeIconKey, { closeX, closeY, closeS, closeS }, closeColor);
        }

        endSvgDraw();

        // Title text
        const std::string & titleText = m_resManager.localeManager().get(m_dialog.title);
        if (!titleText.empty() && m_titleFont != 0) {
            auto * titleFr = fontRenderer->font(m_titleFont);
            if (titleFr != nullptr && titleFr->program != 0U) {
                const auto textW    = fontRenderer->textWidth(m_titleFont, titleText);
                const auto startX   = Ui::Gl::TextAlign::startXCenter({ 0, 0, cssW, 0 }, textW, g_config.scale);
                const auto baseline = titleFr->metrics.baselineCap(m_resManager.layout().dialog.padding,
                                                                   m_resManager.layout().dialogTitleHeight,
                                                                   g_config.scale);
                auto       verts    = Ui::Gl::FontRenderer::buildTextVerts(*titleFr, titleText, startX, baseline);
                if (!verts.empty()) {
                    drawTextVerts(verts, theme.dialogTitleColor, *titleFr);
                }
            }
        }

        // Content text (multi-line with word wrap)
        std::wstring contentText;
        if (!m_dialog.contentOverride.empty()) {
            contentText = m_dialog.contentOverride;
        } else if (!m_dialog.content.empty()) {
            contentText = Common::Unicode::fromUtf8(m_resManager.localeManager().get(m_dialog.content));
        }
        if (!contentText.empty() && m_font != 0) {
            auto * fr = fontRenderer->font(m_font);
            if (fr != nullptr && fr->program != 0U) {
                const auto & layout = m_resManager.layout();
                m_lineHeight        = fr->metrics.height * m_resManager.theme().dialogLineHeight;

                const fpx_t contentTop = layout.dialog.padding + layout.dialogTitleHeight + layout.dialogTitleMargin;
                const fpx_t contentBot = cssH - layout.dialog.padding - layout.dialogButtonH - layout.dialogTextMargin;
                m_viewportHeight       = contentBot - contentTop;

                // Available text width (reserve scrollbar space using hover extent)
                const fpx_t scrollbarLeftEdge = cssW - layout.dialog.padding - layout.dialogScrollbarHoverRight
                                              - layout.dialogScrollbarHoverW;
                const float maxTextWidth = (scrollbarLeftEdge - layout.dialog.padding) * g_config.scale;

                // Word-wrap: split on \n first, then wrap long lines
                auto wrappedLines = Ui::Gl::FontRenderer::wrapText(*fr, contentText, maxTextWidth);

                const fpx_t prevContentH = m_contentHeight;
                m_contentHeight          = m_lineHeight * static_cast<fpx_t>(wrappedLines.size());

                if (m_contentHeight != prevContentH) {
                    std::cout << "[Dialog] contentHeight changed: " << prevContentH << " -> " << m_contentHeight
                              << " lines=" << wrappedLines.size() << " maxTextW=" << maxTextWidth
                              << " viewportH=" << m_viewportHeight << std::endl;
                }

                // Clamp scroll offset
                const bool scrollable = (m_contentHeight > m_viewportHeight);
                if (scrollable) {
                    const fpx_t maxScroll = m_contentHeight - m_viewportHeight;
                    m_scrollTarget        = std::clamp(m_scrollTarget, 0.0F, maxScroll);
                    m_scrollOffset        = std::clamp(m_scrollOffset, 0.0F, maxScroll);
                } else {
                    m_scrollOffset = 0;
                    m_scrollTarget = 0;
                }

                // Enable scissor clipping when content overflows
                if (scrollable) {
                    glEnable(GL_SCISSOR_TEST);
                    const int scissorX = 0;
                    const int scissorW = static_cast<int>(m_width);
                    const int scissorY = static_cast<int>((cssH - contentBot) * g_config.scale);
                    const int scissorH = static_cast<int>(m_viewportHeight * g_config.scale);
                    glScissor(scissorX, scissorY, scissorW, scissorH);
                }

                // Batch all lines into a single vertex buffer
                const float        startX  = layout.dialog.padding * g_config.scale;
                fpx_t              cursorY = contentTop - m_scrollOffset;
                std::vector<float> verts;
                verts.reserve(contentText.size() * 24);

                for (const auto & line : wrappedLines) {
                    const auto baseline = fr->metrics.baseline(cursorY, m_lineHeight, g_config.scale);
                    Ui::Gl::FontRenderer::appendTextVerts(verts, *fr, line, startX, baseline);
                    cursorY += m_lineHeight;
                }

                if (!verts.empty()) {
                    drawTextVerts(verts, theme.dialog.fg, *fr);
                }

                if (scrollable) {
                    glDisable(GL_SCISSOR_TEST);
                }

                // Scrollbar
                if (scrollable) {
                    const bool                    hovered  = m_scrollbarHovered;
                    const Ui::Res::Type::border_t border   = hovered ? layout.dialogScrollbarHoverBorder
                                                                     : layout.dialogScrollbarBorder;
                    const fpx_t                   minThumb = hovered ? layout.dialogScrollbarHoverMinThumb
                                                                     : layout.dialogScrollbarMinThumb;

                    // Hit-test area always uses hover dimensions
                    const fpx_t hitX = cssW - layout.dialog.padding - layout.dialogScrollbarHoverRight
                                     - layout.dialogScrollbarHoverW;
                    m_trackBound = { hitX, contentTop, layout.dialogScrollbarHoverW, m_viewportHeight };

                    // Visual position from current state CSS values (centered by design)
                    const fpx_t drawRight = hovered ? layout.dialogScrollbarHoverRight : layout.dialogScrollbarRight;
                    const fpx_t drawW     = hovered ? layout.dialogScrollbarHoverW : layout.dialogScrollbarW;
                    const fpx_t drawX     = cssW - layout.dialog.padding - drawRight - drawW;

                    const fpx_t thumbRatio  = m_viewportHeight / m_contentHeight;
                    const fpx_t thumbH      = std::max(minThumb, m_viewportHeight * thumbRatio);
                    const fpx_t thumbTravel = m_viewportHeight - thumbH;
                    const fpx_t scrollRatio = m_scrollOffset / (m_contentHeight - m_viewportHeight);
                    m_thumbBound            = { hitX,
                                                contentTop + thumbTravel * scrollRatio,
                                                layout.dialogScrollbarHoverW,
                                                thumbH };

                    const Ui::Res::Type::bound_t drawTrack = { drawX, contentTop, drawW, m_viewportHeight };
                    const Ui::Res::Type::bound_t drawThumb = { drawX, m_thumbBound.y, drawW, thumbH };

                    m_rounded.begin(m_width, m_height, g_config.scale);
                    const Ui::Color & thumbColor    = m_thumbHovered ? theme.dialogScrollbarThumbHover
                                                                     : theme.dialogScrollbarThumb;
                    const bool        atTop         = (drawThumb.y <= drawTrack.y);
                    const bool        atBottom      = (drawThumb.y + drawThumb.h >= drawTrack.y + drawTrack.h);
                    const Ui::Color   thumbBgTop    = atTop ? theme.dialog.bg : theme.dialogScrollbarTrack;
                    const Ui::Color   thumbBgBottom = atBottom ? theme.dialog.bg : theme.dialogScrollbarTrack;

                    m_rounded.draw(drawTrack, border, { theme.dialogScrollbarTrack, theme.dialog.bg });

                    if (atTop == atBottom) {
                        m_rounded.draw(drawThumb, border, { thumbColor, thumbBgTop });
                    } else {
                        const Ui::Res::Type::border_t topBorder    = { border.topLeft, border.topRight, 0, 0 };
                        const Ui::Res::Type::border_t bottomBorder = { 0, 0, border.bottomLeft, border.bottomRight };
                        const fpx_t                   halfH        = drawThumb.h / 2;
                        const Ui::Res::Type::bound_t  topHalf      = { drawThumb.x, drawThumb.y, drawThumb.w, halfH };
                        const Ui::Res::Type::bound_t  bottomHalf   = { drawThumb.x,
                                                                       drawThumb.y + halfH,
                                                                       drawThumb.w,
                                                                       drawThumb.h - halfH };
                        m_rounded.draw(topHalf, topBorder, { thumbColor, thumbBgTop });
                        m_rounded.draw(bottomHalf, bottomBorder, { thumbColor, thumbBgBottom });
                    }
                    Ui::Gl::Rounded::end();
                }
            }
        }

        // Link text (InfoLink)
        if (m_dialog.type == Ui::Res::Type::DialogType::InfoLink && !m_dialog.link.empty() && m_font != 0) {
            const std::string & linkText = m_resManager.localeManager().get(m_dialog.link);
            if (!linkText.empty()) {
                auto * fr = fontRenderer->font(m_font);
                if (fr != nullptr && fr->program != 0U) {
                    const fpx_t lineH = fr->metrics.height * m_resManager.theme().dialogLineHeight;
                    const fpx_t linkY = cssH - m_resManager.layout().dialog.padding
                                      - m_resManager.layout().dialogButtonH - m_resManager.layout().dialogButtonPad
                                      - lineH;
                    const float startX   = m_resManager.layout().dialog.padding * g_config.scale;
                    const auto  baseline = fr->metrics.baseline(linkY, lineH, g_config.scale);
                    auto        verts    = Ui::Gl::FontRenderer::buildTextVerts(*fr, linkText, startX, baseline);
                    if (!verts.empty()) {
                        drawTextVerts(verts, theme.dialogLinkColor, *fr);
                    }
                }
            }
        }

        // Button text
        if (m_font != 0) {
            auto * fr = fontRenderer->font(m_font);
            if (fr != nullptr && fr->program != 0U) {
                for (const auto & btn : m_buttons) {
                    const std::string & text = m_resManager.localeManager().get(btn.label);
                    if (text.empty()) {
                        continue;
                    }

                    const bool                   active   = (btn.state == Ui::Render::UiElementState::Active);
                    const Ui::Res::Type::bound_t btnBound = active
                                                          ? shiftedBound(btn.bound,
                                                                         m_resManager.layout().dialogButtonShift)
                                                          : btn.bound;
                    const auto                   tw       = fontRenderer->textWidth(m_font, text);
                    const auto                   startX = Ui::Gl::TextAlign::startXCenter(btnBound, tw, g_config.scale);
                    const auto baseline = fr->metrics.baselineCap(btnBound.y, btnBound.h, g_config.scale);
                    auto       verts    = Ui::Gl::FontRenderer::buildTextVerts(*fr, text, startX, baseline);

                    Ui::Color fg;
                    if (btn.state == Ui::Render::UiElementState::Active) {
                        fg = theme.dialogButtonActive.fg;
                    } else if (btn.state == Ui::Render::UiElementState::Hovered) {
                        fg = theme.dialogButtonHover.fg;
                    } else if (btn.primary) {
                        fg = theme.dialogButtonPrimary.fg;
                    } else {
                        fg = theme.dialogButton.fg;
                    }

                    if (!verts.empty()) {
                        drawTextVerts(verts, fg, *fr);
                    }
                }
            }
        }
    }

    static Ui::Res::Type::bound_t shiftedBound(const Ui::Res::Type::bound_t & bound, fpx_t shift)
    {
        return { bound.x + shift, bound.y + shift, bound.w, bound.h };
    }

    Ui::Res::Type::dialog_t               m_dialog;
    Ui::font_handle_t                     m_font      = 0;
    Ui::font_handle_t                     m_titleFont = 0;
    std::string                           m_iconKey;
    std::string                           m_closeIconKey;
    std::vector<dialog_button_t>          m_buttons;
    DialogCloseFn                         m_onClose;
    Ui::Res::Type::bound_t                m_closeBound {};
    Ui::Render::UiElementState            m_closeState = Ui::Render::UiElementState::None;
    fpx_t                                 m_scrollOffset {};
    fpx_t                                 m_scrollTarget {};
    fpx_t                                 m_contentHeight {};
    fpx_t                                 m_viewportHeight {};
    fpx_t                                 m_lineHeight {};
    Ui::Res::Type::bound_t                m_trackBound {};
    Ui::Res::Type::bound_t                m_thumbBound {};
    bool                                  m_scrollbarHovered {};
    bool                                  m_thumbHovered {};
    bool                                  m_draggingThumb {};
    fpx_t                                 m_dragStartY {};
    fpx_t                                 m_dragStartScroll {};
    std::chrono::steady_clock::time_point m_lastFrameTime {};
    RenderRequestFn                       m_renderRequest;

    // Keyboard animation state
    size_t                                m_keyboardFocus = Ui::INVALID_ID;
    int                                   m_keyAnimationPhase {};
    size_t                                m_keyAnimationIndex {};
    KeyAnimationTarget                    m_keyAnimationTarget = KeyAnimationTarget::None;
    std::chrono::steady_clock::time_point m_keyAnimationStart {};
};

} // namespace Ui::Render::Popup
