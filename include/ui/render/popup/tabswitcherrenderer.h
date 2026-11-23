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

#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/fontrenderer.h"
#include "ui/gl/localglew.h"
#include "ui/gl/rounded.h"
#include "ui/gl/svgdocuments.h"
#include "ui/gl/svgrenderer.h"
#include "ui/gl/textalign.h"
#include "ui/render/elementevent.h"
#include "ui/render/eventkind.h"
#include "ui/render/popup/popuprendererbase.h"
#include "ui/render/rowcloseboundof.h"
#include "ui/render/rowsscrolledof.h"
#include "ui/render/rowsshownof.h"
#include "ui/render/scrollbar.h"
#include "ui/render/uielement.h"
#include "ui/res/key/element.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/layout.h"
#include "ui/res/type/region.h"
#include "ui/res/type/theme.h"
#include "ui/revealedoffsetof.h"
#include "ui/tab.h"
#include "ui/type.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace Ui::Render::Popup {

// Flip to true to trace the Ctrl+Tab list: each pointer move, press, release and wheel turn it
// takes, with what it made of it (the row hovered, the gutter, the thumb held, the first row);
// each close with the event behind it and each switch through it (Shell); its window taken down
// (WindowManager)
constexpr bool TAB_SWITCHER_DEBUG = false;

// The Ctrl+Tab list: a row per tab, most recent first - its label, and dimmed after it
// its description (a file's folder) - the selected one highlighted, and a close button on
// the right of each row, as each tab has one in the strip. Its sizes are layout.json's
// tab-switcher, -item, -item:active and -close, its font and colours the theme's blocks of
// the same names, tab-switcher-close(:selected/:hover/:active) and tab-switcher-description(:active).
// The rows come fitted to its width (UiLayout::fitTabSwitcherRows); which one is selected is
// the shell's (Ui::TabSwitch), the pointer only offering it the row it is over. A click on
// a row is a click on that tab, and one on its close button a click on that tab's: the
// element it reports is the tab's
class TabSwitcherRenderer final : public PopupRendererBase {
public:
    TabSwitcherRenderer(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager & resManager, std::vector<Ui::tab_t> rows)
        : PopupRendererBase(std::move(makeCurrent), resManager)
        , m_rows(std::move(rows))
        , m_font(m_fontRenderer.createFont(resManager.theme().tabSwitcherFont))
        , m_closeIconKey(
              Ui::Gl::SvgDocuments::ensureLoaded(resManager.resPath().icon(resManager.layout().tabSwitcherCloseIcon)))
    {
    }

    ~TabSwitcherRenderer() override { cleanup(); }

    // The highlighted row, scrolled the least that shows it
    void setSelectedRow(std::size_t row)
    {
        m_selectedRow = row;
        m_firstRow    = Ui::revealedOffsetOf(m_firstRow, row, shownRows());
    }

    // The row the pointer is over; none off every row
    [[nodiscard]] std::optional<std::size_t> hoveredRow() const { return m_hoveredRow; }

    // Its scrollbar's thumb grabbed and the button not yet up: the pointer is the list's
    [[nodiscard]] bool isThumbHeld() const { return m_scrollBar.isDragging(); }

    // The first row shown
    [[nodiscard]] std::size_t firstRow() const { return m_firstRow; }

    // The rows shown from `row` on, kept among those that leave the list full: a list opened
    // again over itself - a row closed from it - stays scrolled as it was
    void scrollTo(std::size_t row) { setFirstRow(static_cast<std::ptrdiff_t>(row)); }

    // The room on the right a list with more tabs than rows keeps for its scrollbar, its rows
    // fitted and drawn left of it, as a dialog's text is of its bar: the bar at its widest and
    // the highlight's margin, which also stands between the bar and the list's edge
    [[nodiscard]] static fpx_t scrollGutterOf(const Ui::Res::Type::layout_t & layout)
    {
        return layout.scrollbarHoverW + layout.tabSwitcherItemActive.margin;
    }

    // The room on a row's right its close button keeps, the row's text fitted left of it: the
    // button, and `right` past it to the row's edge
    [[nodiscard]] static fpx_t closeRoomOf(const Ui::Res::Type::layout_t & layout)
    {
        return Ui::Render::rowCloseBoundOf(layout.tabSwitcherClose, {}).w + layout.tabSwitcherClose.right;
    }

    bool render() override
    {
        const auto & theme         = m_resManager.theme();
        const bool   premultiplied = beginRender(theme.tabSwitcher.bg);
        if (m_width <= 0 || m_height <= 0) {
            return true;
        }

        const auto &    layout  = m_resManager.layout();
        const fpx_t     width   = toCss(m_width);
        const fpx_t     rowH    = rowHeight();
        const fpx_t     padH    = layout.tabSwitcherItem.padding;
        const Ui::Color bgColor = premultiplied ? Ui::Color::TransparentBlack() : theme.tabSwitcher.bg;
        const fpx_t     rowsW   = rowsWidth();

        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_rounded.begin(m_width, m_height, g_config.scale);
        m_rounded.draw({ .w = width, .h = toCss(m_height) },
                       layout.tabSwitcher.border,
                       { .fg = theme.tabSwitcher.bg, .bg = bgColor });
        if (isShown(m_selectedRow)) {
            const fpx_t inset = layout.tabSwitcherItemActive.margin;
            m_rounded.draw(
                { .x = inset, .y = rowY(m_selectedRow) + inset, .w = rowsW - (inset * 2), .h = rowH - (inset * 2) },
                layout.tabSwitcherItemActive.border,
                { .fg = theme.tabSwitcherItemActive.bg, .bg = bgColor });
        }
        // A close button's background only where it has one of its own (not inherit), so the
        // highlight moving onto or off its row never fades it. Premultiplied over what is drawn
        // under it, the highlight or the list, so a fade's translucent colour blends in rather
        // than standing opaque (Rounded's opaque path, which inheritBg's ground is for)
        for (std::size_t row = m_firstRow; isShown(row); ++row) {
            const Ui::Color closeBg = drawnCloseColorsOf(row, theme).bg;
            if (closeBg.a() > 0 && !closeBg.isInherit()) {
                m_rounded.draw(closeBoundOf(row),
                               layout.tabSwitcherClose.border,
                               { .fg = closeBg, .bg = Ui::Color::TransparentBlack() });
            }
        }
        Ui::Gl::Rounded::end();

        // More tabs than rows: the scrollbar says where the rows shown stand among them, in the
        // gutter right of them, at its widest the highlight's margin in from the list's edge
        // (ScrollBar sets a bar its `right` in from the area's), clear of its rounded corners
        if (isScrolling()) {
            const Ui::Res::Type::border_t & corners = layout.tabSwitcher.border;
            const fpx_t                     inset   = layout.tabSwitcherItemActive.margin;
            m_scrollBar.setGeometry({ .y = corners.topRight,
                                      .w = width - inset + layout.scrollbarHoverRight,
                                      .h = toCss(m_height) - corners.topRight - corners.bottomRight },
                                    rowH * static_cast<fpx_t>(m_rows.size()),
                                    rowH * static_cast<fpx_t>(shownRows()));
            drawScrollBar(m_scrollBar, scrollValue(), theme.tabSwitcher.bg);
        }

        // Each close button's icon, its margin in, tinted where its fade stands
        if (!m_closeIconKey.empty() && Ui::Gl::SvgDocuments::isLoaded(m_closeIconKey)) {
            const Ui::Res::Type::region_t & close = layout.tabSwitcherClose;
            beginSvgDraw();
            for (std::size_t row = m_firstRow; isShown(row); ++row) {
                const Ui::Res::Type::bound_t bound = closeBoundOf(row);
                m_svgRenderer.drawTinted(m_closeIconKey,
                                         { .x = (bound.x + close.margin) * g_config.scale,
                                           .y = (bound.y + close.margin) * g_config.scale,
                                           .w = close.height * g_config.scale,
                                           .h = close.height * g_config.scale },
                                         drawnCloseColorsOf(row, theme).fg);
            }
            endSvgDraw();
        }

        auto * font = m_fontRenderer.font(m_font);
        if (font == nullptr || font->program == 0U) {
            return true;
        }
        for (std::size_t row = m_firstRow; isShown(row); ++row) {
            const Ui::tab_t &            tab        = m_rows.at(row);
            const bool                   isSelected = row == m_selectedRow;
            const Ui::Res::Type::bound_t box        = { .y = rowY(row), .w = width, .h = rowH };
            const auto                   baseline   = font->metrics.baselineCap(box.y, box.h, g_config.scale);

            auto labelVerts = Ui::Gl::FontRenderer::buildTextVerts(
                *font,
                tab.label,
                Ui::Gl::TextAlign::startXLeft(box, padH, g_config.scale),
                baseline);
            if (!labelVerts.empty()) {
                drawTextVerts(labelVerts,
                              isSelected ? theme.tabSwitcherItemActive.fg : theme.tabSwitcherItem.fg,
                              *font);
            }
            if (tab.description.empty()) {
                continue;
            }
            const Ui::Res::Type::bound_t after            = { .x = padH + m_fontRenderer.textWidth(m_font, tab.label),
                                                              .y = box.y,
                                                              .w = width,
                                                              .h = rowH };
            auto                         descriptionVerts = Ui::Gl::FontRenderer::buildTextVerts(
                *font,
                tab.description,
                Ui::Gl::TextAlign::startXLeft(after, padH, g_config.scale),
                baseline);
            if (!descriptionVerts.empty()) {
                drawTextVerts(descriptionVerts,
                              isSelected ? theme.tabSwitcherDescriptionActiveColor : theme.tabSwitcherDescriptionColor,
                              *font);
            }
        }
        return true;
    }

    // A thumb held follows the pointer; a row is hovered only within the rows' rectangle, never
    // over the bar's gutter, so the highlight never jumps there, and a close button within its
    // own square. Whether the bar's or a close button's look changed
    bool onMouseMove(int x, int y) override
    {
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);
        if (m_scrollBar.isDragging()) {
            const bool isMoved = setScrollValue(m_scrollBar.valueAt(cssY));
            tracePointer("move", cssX, cssY);
            return isMoved;
        }
        m_hoveredRow              = rowAt(x, y);
        const bool isCloseChanged = hoverClose(x, y);
        tracePointer("move", cssX, cssY);
        const bool isBarChanged = isScrolling() && m_scrollBar.onMouseMove(cssX, cssY, scrollValue());
        return isCloseChanged || isBarChanged;
    }

    // No row or close button hovered once the pointer is off the list, nor a close button
    // pressed - the dialog's way; a thumb held stays held, as the pointer is its until the button
    // comes up (WindowManager keeps sending it the moves and the release)
    bool onMouseLeave() override
    {
        const bool wasOnBar   = m_scrollBar.isHovered() || m_scrollBar.isDragging();
        const bool wasOnClose = m_hoveredClose.has_value() || m_pressedClose.has_value();
        m_hoveredRow          = std::nullopt;
        m_hoveredClose        = std::nullopt;
        m_pressedClose        = std::nullopt;
        if (!m_scrollBar.isDragging()) {
            m_scrollBar.clear();
        }
        if constexpr (TAB_SWITCHER_DEBUG) {
            std::cout << "[TabSwitch] leave" << (wasOnBar ? ", off the bar" : "")
                      << (wasOnClose ? ", off a close button" : "") << std::endl;
        }
        return wasOnBar || wasOnClose;
    }

    // A left press on a row's close button presses it, its release over it closing the tab; one
    // anywhere in the bar's gutter grabs the thumb, or pages the track by a screenful of rows -
    // ScrollBar decides by the height alone
    Ui::Render::element_event_t onMousePress(int                     x,
                                             int                     y,
                                             Ui::Window::MouseButton button,
                                             int /*clickCount*/,
                                             Ui::Window::key_modifiers_t /*modifiers*/) override
    {
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);
        m_pressedClose   = button == Ui::Window::MouseButton::Left ? closeAt(x, y) : std::nullopt;
        if (m_pressedClose.has_value()) {
            tracePointer("press on a close button", cssX, cssY);
            return { .changed = true };
        }
        if (button != Ui::Window::MouseButton::Left || !isOverGutter(cssX)) {
            tracePointer("press, not the bar's", cssX, cssY);
            return {};
        }
        const fpx_t value   = scrollValue();
        const bool  isMoved = setScrollValue(m_scrollBar.press(cssY, value, value));
        tracePointer("press on the bar", cssX, cssY);
        return { .changed = isMoved || m_scrollBar.isDragging() };
    }

    // The wheel - a touchpad's two fingers among them - scrolls the rows as a dock's: a notch at
    // least a row. The rows moved under a pointer that did not, so it hovers the row now under it,
    // as a move there would
    Ui::Render::element_event_t onScroll(int x, int y, fpx_t deltaY) override
    {
        const int  rows           = Ui::Render::rowsScrolledOf(deltaY, rowHeight(), m_resManager.input());
        const bool isMoved        = rows != 0 && setFirstRow(static_cast<std::ptrdiff_t>(m_firstRow) + rows);
        m_hoveredRow              = rowAt(x, y);
        const bool isCloseChanged = hoverClose(x, y);
        if constexpr (TAB_SWITCHER_DEBUG) {
            std::cout << "[TabSwitch] wheel " << deltaY << ": " << rows << " rows, first=" << m_firstRow << std::endl;
        }
        return { .changed = isMoved || isCloseChanged };
    }

    // A left click on a row is one on its tab, and on its close button - pressed there and let go
    // of over it - one on its tab's close button; let go of anywhere else after that press, or in
    // the gutter, or letting go of the thumb, is none
    Ui::Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button) override
    {
        if (m_scrollBar.isDragging()) {
            // Let go over a row, that row is hovered, as a move there would have made it; the
            // bar's hover follows where the pointer is
            m_scrollBar.release();
            m_hoveredRow = rowAt(x, y);
            std::ignore  = hoverClose(x, y);
            std::ignore  = m_scrollBar.onMouseMove(toCss(x), toCss(y), scrollValue());
            tracePointer("release of the thumb", toCss(x), toCss(y));
            return { .changed = true };
        }
        if (button == Ui::Window::MouseButton::Left && m_pressedClose.has_value()) {
            const std::size_t pressed = *m_pressedClose;
            m_pressedClose            = std::nullopt;
            if (closeAt(x, y) != pressed) {
                tracePointer("release off the close button pressed", toCss(x), toCss(y));
                return { .changed = true };
            }
            tracePointer("release on its close button", toCss(x), toCss(y));
            return { .type    = Ui::Render::UiElementType::TabClose,
                     .id      = m_rows.at(pressed).id,
                     .event   = Ui::Render::EventKind::LeftClick,
                     .x       = toCss(x),
                     .y       = toCss(y),
                     .changed = true };
        }
        const std::optional<std::size_t> row = rowAt(x, y);
        tracePointer(row.has_value() ? "release on a row" : "release off every row", toCss(x), toCss(y));
        if (button != Ui::Window::MouseButton::Left || !row.has_value()) {
            return {};
        }
        return { .type    = Ui::Render::UiElementType::Tab,
                 .id      = m_rows.at(row.value()).id,
                 .event   = Ui::Render::EventKind::LeftClick,
                 .x       = toCss(x),
                 .y       = toCss(y),
                 .changed = true };
    }

private:
    std::vector<Ui::tab_t>     m_rows;            // most recent first, fitted to the width
    Ui::font_handle_t          m_font        = 0; // the theme's tab-switcher-item font
    std::size_t                m_selectedRow = 0;
    std::size_t                m_firstRow    = 0; // the first shown, while more rows than fit
    std::optional<std::size_t> m_hoveredRow;      // what the pointer offers
    std::string                m_closeIconKey;    // tab-switcher-close's icon, loaded once
    std::optional<std::size_t> m_hoveredClose;    // the row whose close button the pointer is over
    std::optional<std::size_t> m_pressedClose;    // the row whose close button a left press went down on
    std::vector<bool>          m_isCloseOnSelected = std::vector<bool>(
        m_rows.size());                                 // per row: its close button last drawn on the highlight
    Ui::Render::ScrollBar m_scrollBar { m_resManager }; // where the rows shown stand among them all

    // layout.json's tab-switcher-item height
    [[nodiscard]] fpx_t rowHeight() const { return m_resManager.layout().tabSwitcherItem.height; }

    // As many rows as the window is tall
    [[nodiscard]] std::size_t shownRows() const { return Ui::Render::rowsShownOf(toCss(m_height), rowHeight()); }

    // More tabs than rows: the list scrolls, its bar drawn and taking the pointer
    [[nodiscard]] bool isScrolling() const { return m_rows.size() > shownRows(); }

    // The first row shown at `row`, kept among those that leave the list full; whether it moved
    bool setFirstRow(std::ptrdiff_t row)
    {
        const std::ptrdiff_t most = std::max<std::ptrdiff_t>(
            static_cast<std::ptrdiff_t>(m_rows.size()) - static_cast<std::ptrdiff_t>(shownRows()),
            0);
        const auto first = static_cast<std::size_t>(std::clamp<std::ptrdiff_t>(row, 0, most));
        if (first == m_firstRow) {
            return false;
        }
        m_firstRow = first;
        return true;
    }

    // The bar's value: the rows scrolled past, in CSS px, as ScrollBar measures it
    [[nodiscard]] fpx_t scrollValue() const { return rowHeight() * static_cast<fpx_t>(m_firstRow); }

    // A value of the bar's as the row nearest it; whether the rows moved
    bool setScrollValue(fpx_t value)
    {
        const fpx_t rowH = rowHeight();
        return rowH > 0.0F && setFirstRow(std::lround(value / rowH));
    }

    // A row that is drawn: one of them, and scrolled into the window
    [[nodiscard]] bool isShown(std::size_t row) const
    {
        return row < m_rows.size() && row >= m_firstRow && row < m_firstRow + shownRows();
    }

    // Its top in CSS px, from the first row shown
    [[nodiscard]] fpx_t rowY(std::size_t row) const { return static_cast<fpx_t>(row - m_firstRow) * rowHeight(); }

    // TAB_SWITCHER_DEBUG's line for a pointer event: where (CSS px), the rows' right edge, over the
    // gutter or not, the thumb held or not, the rows hovered, first shown and selected, and the
    // rows whose close button is hovered and pressed
    void tracePointer(std::string_view what, fpx_t cssX, fpx_t cssY) const
    {
        if constexpr (TAB_SWITCHER_DEBUG) {
            const auto rowName = [](const std::optional<std::size_t> & row) {
                return row.has_value() ? std::to_string(*row) : std::string("none");
            };
            std::cout << "[TabSwitch] " << what << " at " << cssX << "," << cssY << " rows end " << rowsWidth()
                      << (isOverGutter(cssX) ? " gutter" : "") << (m_scrollBar.isDragging() ? " thumb held" : "")
                      << " hovered=" << rowName(m_hoveredRow) << " first=" << m_firstRow
                      << " selected=" << m_selectedRow << " close hovered=" << rowName(m_hoveredClose)
                      << " pressed=" << rowName(m_pressedClose) << std::endl;
        }
    }

    // Where a row's close button stands, CSS px: on the right of the row, left of any gutter
    [[nodiscard]] Ui::Res::Type::bound_t closeBoundOf(std::size_t row) const
    {
        return Ui::Render::rowCloseBoundOf(m_resManager.layout().tabSwitcherClose,
                                           { .x = 0.0F, .y = rowY(row), .w = rowsWidth(), .h = rowHeight() });
    }

    // The row whose close button is at (`x`, `y`) (window physical px); none off every one
    [[nodiscard]] std::optional<std::size_t> closeAt(int x, int y) const
    {
        const std::optional<std::size_t> row = rowAt(x, y);
        if (!row.has_value() || !closeBoundOf(*row).contains(toCss(x), toCss(y))) {
            return std::nullopt;
        }
        return row;
    }

    // The close button at (`x`, `y`) hovered, none off every one; whether that changed its look
    [[nodiscard]] bool hoverClose(int x, int y)
    {
        const std::optional<std::size_t> close = closeAt(x, y);
        if (close == m_hoveredClose) {
            return false;
        }
        m_hoveredClose = close;
        return true;
    }

    // A row's close button's colours where their fade stands: pressed while the pointer is still
    // over it, hovered, on the highlighted row - inverted as the row's text is - or none of them;
    // its background inherit where it has none of its own, which fades as none (the tab strip's
    // close button's way), drawn premultiplied. Only its own hover and press fade: its row's
    // highlight coming or going changes it at once, as it does the row's text
    [[nodiscard]] Ui::Res::Type::color_pair_t drawnCloseColorsOf(std::size_t row, const Ui::Res::Type::theme_t & theme)
    {
        const bool                          isHovered  = m_hoveredClose == row;
        const bool                          isSelected = row == m_selectedRow;
        const Ui::Res::Type::color_pair_t & state      = [&]() -> const Ui::Res::Type::color_pair_t & {
            if (isHovered) {
                return m_pressedClose == row ? theme.tabSwitcherCloseActive : theme.tabSwitcherCloseHover;
            }
            return isSelected ? theme.tabSwitcherCloseSelected : theme.tabSwitcherClose;
        }();
        const id_t id = m_rows.at(row).id;
        if (isSelected != m_isCloseOnSelected.at(row)) {
            m_isCloseOnSelected.at(row) = isSelected;
            if (!isHovered) {
                m_animator.setColors(id, state);
            }
        }
        return colorsOf(id, state, Ui::Res::Key::ElementKey::TabSwitcherClose);
    }

    // The rows' width, CSS px: the list's, less the gutter its scrollbar keeps while it scrolls
    [[nodiscard]] fpx_t rowsWidth() const
    {
        return toCss(m_width) - (isScrolling() ? scrollGutterOf(m_resManager.layout()) : 0.0F);
    }

    // Over the gutter, the bar's: a press there works it, and no row is hovered or clicked
    [[nodiscard]] bool isOverGutter(fpx_t cssX) const { return isScrolling() && cssX >= rowsWidth(); }

    // The row at (`x`, `y`) (window physical px), hit within the rows' own rectangle - never in
    // the gutter right of them; none off every row
    [[nodiscard]] std::optional<std::size_t> rowAt(int x, int y) const
    {
        const fpx_t cssX = toCss(x);
        if (cssX < 0 || cssX >= rowsWidth()) {
            return std::nullopt;
        }
        const fpx_t cssY = toCss(y);
        const fpx_t rowH = rowHeight();
        if (cssY < 0 || rowH <= 0) {
            return std::nullopt;
        }
        const std::size_t row = m_firstRow + static_cast<std::size_t>(cssY / rowH);
        if (!isShown(row)) {
            return std::nullopt;
        }
        return row;
    }
};

} // namespace Ui::Render::Popup
