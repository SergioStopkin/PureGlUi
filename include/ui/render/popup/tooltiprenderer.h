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
#include "ui/res/resmanager.h"
#include "ui/type.h"

#include <string>
#include <utility>

namespace Ui::Render::Popup {

// The tooltip, a toolbar or menu-bar button's or the one a content surface asks for:
// res "tooltip" box and one line of text. Takes no input - it sits beside its anchor
class TooltipRenderer final : public PopupRendererBase {
public:
    TooltipRenderer(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager & resManager, std::string text)
        : PopupRendererBase(std::move(makeCurrent), resManager)
        , m_text(std::move(text))
        , m_font(m_fontRenderer.createFont(resManager.theme().tooltipFont))
    {
    }

    ~TooltipRenderer() override { cleanup(); }

    // A kept tooltip says something else: its next render draws it
    void setText(std::string text) { m_text = std::move(text); }

    bool render() override
    {
        const Ui::Res::Type::color_pair_t & colors        = m_resManager.theme().tooltip;
        const Ui::Res::Type::region_t &     box           = m_resManager.layout().tooltip;
        const bool                          premultiplied = beginRender(colors.bg);
        if (m_width <= 0 || m_height <= 0) {
            return true;
        }

        const Ui::Res::Type::bound_t bound = { 0, 0, toCss(m_width), toCss(m_height) };
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_rounded.begin(m_width, m_height, g_config.scale);
        m_rounded.draw(bound, box.border, { colors.bg, premultiplied ? Ui::Color::TransparentBlack() : colors.bg });
        Ui::Gl::Rounded::end();

        auto * font = m_fontRenderer.font(m_font);
        if (font == nullptr || font->program == 0U) {
            return true;
        }
        const auto startX   = Ui::Gl::TextAlign::startXLeft(bound, box.padding, g_config.scale);
        const auto baseline = font->metrics.baselineCap(bound.y, bound.h, g_config.scale);
        auto       verts    = Ui::Gl::FontRenderer::buildTextVerts(*font, m_text, startX, baseline);
        if (!verts.empty()) {
            drawTextVerts(verts, colors.fg, *font);
        }
        return true;
    }

    bool onMouseMove(int /*x*/, int /*y*/) override { return false; }

    bool onMouseLeave() override { return false; }

    Ui::Render::element_event_t onMousePress(int /*x*/,
                                             int /*y*/,
                                             Ui::Window::MouseButton /*button*/,
                                             int /*clickCount*/,
                                             Ui::Window::KeyModifier /*modifiers*/) override
    {
        return {};
    }

    Ui::Render::element_event_t onMouseRelease(int /*x*/, int /*y*/, Ui::Window::MouseButton /*button*/) override
    {
        return {};
    }

private:
    std::string       m_text;
    Ui::font_handle_t m_font = 0;
};

} // namespace Ui::Render::Popup
