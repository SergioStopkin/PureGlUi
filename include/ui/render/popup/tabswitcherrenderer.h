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
#include "ui/gl/textalign.h"
#include "ui/render/elementevent.h"
#include "ui/render/eventkind.h"
#include "ui/render/popup/popuprendererbase.h"
#include "ui/render/uielement.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/tab.h"
#include "ui/type.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace Ui::Render::Popup {

// The Ctrl+Tab list: a row per tab, most recent first - its label, and dimmed after it
// its description (a file's folder) - the selected one highlighted. Its sizes are
// layout.json's tab-switcher, -item and -item:active, its font and colours the theme's
// blocks of the same names and tab-switcher-description(:active). The rows come fitted to its width
// (UiLayout::fitTabSwitcherRows); which one is selected is the shell's (Ui::TabSwitch),
// the pointer only offering it the row it is over. A click on a row is a click on that
// tab: the element it reports is the tab's
class TabSwitcherRenderer final : public PopupRendererBase {
public:
    TabSwitcherRenderer(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager & resManager, std::vector<Ui::tab_t> rows)
        : PopupRendererBase(std::move(makeCurrent), resManager)
        , m_rows(std::move(rows))
        , m_font(m_fontRenderer.createFont(resManager.theme().tabSwitcherFont))
    {
    }

    ~TabSwitcherRenderer() override { cleanup(); }

    // The highlighted row, scrolled the least that shows it
    void setSelectedRow(std::size_t row)
    {
        m_selectedRow           = row;
        const std::size_t shown = shownRows();
        if (row < m_firstRow) {
            m_firstRow = row;
        } else if (shown > 0 && row >= m_firstRow + shown) {
            m_firstRow = row + 1 - shown;
        }
    }

    // The row the pointer is over; none off every row
    [[nodiscard]] std::optional<std::size_t> hoveredRow() const { return m_hoveredRow; }

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

        glDisable(GL_DEPTH_TEST);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_rounded.begin(m_width, m_height, g_config.scale);
        m_rounded.draw({ .w = width, .h = toCss(m_height) },
                       layout.tabSwitcher.border,
                       { .fg = theme.tabSwitcher.bg, .bg = bgColor });
        if (isShown(m_selectedRow)) {
            const fpx_t inset = layout.tabSwitcherItemActive.margin;
            m_rounded.draw(
            { .x = inset, .y = rowY(m_selectedRow) + inset, .w = width - (inset * 2), .h = rowH - (inset * 2) },
            layout.tabSwitcherItemActive.border,
            { .fg = theme.tabSwitcherItemActive.bg, .bg = bgColor });
        }
        Ui::Gl::Rounded::end();

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

    bool onMouseMove(int /*x*/, int y) override
    {
        m_hoveredRow = rowAt(y);
        return false;
    }

    bool onMouseLeave() override
    {
        m_hoveredRow = std::nullopt;
        return false;
    }

    Ui::Render::element_event_t onMousePress(int /*x*/,
                                             int /*y*/,
                                             Ui::Window::MouseButton /*button*/,
                                             int /*clickCount*/,
                                             Ui::Window::key_modifiers_t /*modifiers*/) override
    {
        return {};
    }

    // A left click on a row is one on its tab
    Ui::Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button) override
    {
        const std::optional<std::size_t> row = rowAt(y);
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
    // layout.json's tab-switcher-item height
    [[nodiscard]] fpx_t rowHeight() const { return m_resManager.layout().tabSwitcherItem.height; }

    // As many rows as the window is tall
    [[nodiscard]] std::size_t shownRows() const
    {
        const fpx_t rowH = rowHeight();
        return rowH > 0 ? static_cast<std::size_t>(toCss(m_height) / rowH) : 0;
    }

    // A row that is drawn: one of them, and scrolled into the window
    [[nodiscard]] bool isShown(std::size_t row) const
    {
        return row < m_rows.size() && row >= m_firstRow && row < m_firstRow + shownRows();
    }

    // Its top in CSS px, from the first row shown
    [[nodiscard]] fpx_t rowY(std::size_t row) const { return static_cast<fpx_t>(row - m_firstRow) * rowHeight(); }

    // The row at `y` (window physical px); none off every row
    [[nodiscard]] std::optional<std::size_t> rowAt(int y) const
    {
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

    std::vector<Ui::tab_t>     m_rows;            // most recent first, fitted to the width
    Ui::font_handle_t          m_font        = 0; // the theme's tab-switcher-item font
    std::size_t                m_selectedRow = 0;
    std::size_t                m_firstRow    = 0; // the first shown, while more rows than fit
    std::optional<std::size_t> m_hoveredRow;      // what the pointer offers
};

} // namespace Ui::Render::Popup
