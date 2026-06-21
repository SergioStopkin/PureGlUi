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
#include "ui/backend/gl/fontrenderer.h"
#include "ui/backend/gl/glutil.h"
#include "ui/backend/gl/rounded.h"
#include "ui/backend/gl/svgrenderer.h"
#include "ui/backend/window/iwindow.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/interface/ipopuprenderer.h"
#include "ui/render/uirenderer.h"

#include <memory>
#include <string>
#include <vector>

namespace Ui::Render::Popup {

/**
 * @brief Shared base for popup-type renderers (menu popups, dialogs)
 *
 * Provides common GL setup, text rendering, SVG icon rendering,
 * corner pixel management, and boilerplate Ui::IRenderer overrides.
 */
class PopupRendererBase : public Ui::IPopupRenderer {
public:
    PopupRendererBase(Ui::Backend::Window::IWindow & window, const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
        , m_window(&window)
    {
        m_window->makeCurrent();
        m_uiRender = std::make_unique<Ui::Render::UiRenderer>([w = &window] { w->makeCurrent(); },
                                                              window.bound().w,
                                                              window.bound().h,
                                                              m_resManager);
    }

    ~PopupRendererBase() override = default;

    PopupRendererBase(const PopupRendererBase &)             = delete;
    PopupRendererBase(PopupRendererBase &&)                  = delete;
    PopupRendererBase & operator=(const PopupRendererBase &) = delete;
    PopupRendererBase & operator=(PopupRendererBase &&)      = delete;

    // -- Ui::IPopupRenderer --

    void setAlpha(bool hasAlpha) override { m_rounded.setAlpha(hasAlpha); }

    void setCornerPixels(std::array<std::vector<uint8_t>, 4> pixels, const Ui::Res::Type::border_t & radii) override
    {
        m_rounded.setCornerPixels(std::move(pixels), radii);
    }

    [[nodiscard]] bool hasCornerPixels() const override { return m_rounded.hasCornerPixels(); }

    // -- Ui::IRenderer boilerplate --

    void resize(fpx_t width, fpx_t height) override
    {
        if (m_width != width || m_height != height) {
            m_width  = width;
            m_height = height;
        }
    }

    void move() override { }

    [[nodiscard]] fpx_t width() const override { return m_width; }
    [[nodiscard]] fpx_t height() const override { return m_height; }

    void apply(Ui::Res::Type::Changed changed) override
    {
        if (changed != Ui::Res::Type::Changed::None) { }
    }

    void cleanup() override
    {
        m_uiRender.reset();
        m_window = nullptr;
        m_rounded.cleanup();
    }

    bool onScroll(int, int, fpx_t) override { return false; }

protected:
    // Begin a render frame: clear, viewport, blend, corner underlay.
    // Returns true if premultiplied alpha is active.
    bool beginRender()
    {
        if (!m_uiRender || m_window == nullptr || m_width <= 0 || m_height <= 0) {
            return false;
        }

        m_window->makeCurrent();

        const bool usePremultiplied = m_rounded.hasAlpha() || m_rounded.hasCornerPixels();

        if (usePremultiplied) {
            glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        } else {
            auto clr = m_resManager.theme().dropdown.bg.toGLRGBA();
            glClearColor(clr.at(0), clr.at(1), clr.at(2), clr.at(3));
        }
        glClear(Common::Bit::Or(GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT));

        glViewport(0, 0, static_cast<int>(m_width), static_cast<int>(m_height));

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Backend::Gl::SvgRenderer::setUploadPremultiplied(true);

        m_rounded.drawCorners(m_width, m_height, g_config.scale);

        return usePremultiplied;
    }

    // Draw text vertex buffer using font shader
    void drawTextVerts(const std::vector<float> &                  verts,
                       const Ui::Color &                           color,
                       Ui::Backend::Gl::FontRenderer::font_rec_t & fr) const
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glUseProgram(fr.program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fr.atlas_tex);
        glUniform1i(fr.uTex, 0);
        glUniform1i(fr.uPremultiplied, 0);
        const auto fontProj = Ui::Backend::Gl::Util::orthoProjection(static_cast<fpx_t>(m_width),
                                                                     static_cast<fpx_t>(m_height));
        glUniformMatrix4fv(fr.uProjection, 1, GL_FALSE, fontProj.data());
        auto cf = color.toGLRGBA();
        glUniform4f(fr.uColor, cf.at(0), cf.at(1), cf.at(2), cf.at(3));

        glBindVertexArray(fr.vao);
        glBindBuffer(GL_ARRAY_BUFFER, fr.vbo);
        Ui::Backend::Gl::Util::drawTriangles(verts);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glDisable(GL_FRAMEBUFFER_SRGB);
    }

    void beginSvgDraw()
    {
        m_svgRenderer.begin(m_width, m_height, 1.0F);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    static void endSvgDraw() { Ui::Backend::Gl::SvgRenderer::end(); }

    // -- Shared members --

    const Ui::Res::ResManager &             m_resManager;
    Ui::Backend::Window::IWindow *          m_window = nullptr;
    std::unique_ptr<Ui::Render::UiRenderer> m_uiRender;
    fpx_t                                   m_width  = 0;
    fpx_t                                   m_height = 0;
    Ui::Backend::Gl::SvgRenderer            m_svgRenderer;
    Ui::Backend::Gl::Rounded                m_rounded;
};

} // namespace Ui::Render::Popup
