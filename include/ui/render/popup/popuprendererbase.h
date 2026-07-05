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
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/fontrenderer.h"
#include "ui/gl/glutil.h"
#include "ui/gl/rounded.h"
#include "ui/gl/svgrenderer.h"
#include "ui/interface/irenderer.h"
#include "ui/type.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Ui::Render::Popup {

/**
 * @brief Shared base for popup-type renderers (menu popups, dialogs)
 *
 * Provides common GL setup, text rendering, SVG icon rendering,
 * corner pixel management, and boilerplate Ui::IRenderer overrides.
 */
class PopupRendererBase : public Ui::IRenderer, private Common::NonCopyable {
public:
    // The window layer makes its context current before constructing/driving the
    // renderer; the renderer only needs an abstract "make current" capability
    // (passed to its FontRenderer for lazy glyph upload + teardown).
    PopupRendererBase(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
        , m_makeCurrent(std::move(makeCurrent))
        , m_fontRenderer(m_makeCurrent, resManager.resPath().fontDir())
    {
    }

    ~PopupRendererBase() override
    {
        // A dtor body runs before members are destroyed: make our window's GL
        // context current here so the Rounded/Svg/Font members below glDelete in
        // THIS context, not whatever is current at destruction time. Contexts are
        // not shared, so deleting in the wrong one destroys another window's GL
        // objects of the same numeric ids and blanks it.
        if (m_makeCurrent) {
            m_makeCurrent();
        }
    }

    // -- Software-rounded corner support (popup/dialog corners over parent pixels) --

    void setAlpha(bool hasAlpha) { m_rounded.setAlpha(hasAlpha); }

    void setCornerPixels(std::array<std::vector<uint8_t>, 4> pixels, const Ui::Res::Type::border_t & radii)
    {
        m_rounded.setCornerPixels(std::move(pixels), radii);
    }

    [[nodiscard]] bool hasCornerPixels() const { return m_rounded.hasCornerPixels(); }

    // -- Ui::IRenderer boilerplate --

    void resize(fpx_t width, fpx_t height) override
    {
        if (m_width != width || m_height != height) {
            m_width  = width;
            m_height = height;
        }
    }

    void apply(Ui::Res::Type::Changed changed) override
    {
        if (changed != Ui::Res::Type::Changed::None) { }
    }

    void cleanup() override
    {
        if (m_makeCurrent) {
            m_makeCurrent();
        }
        m_rounded.cleanup();
    }

    bool onScroll(int /*x*/, int /*y*/, fpx_t /*deltaY*/) override { return false; }

protected:
    // Begin a render frame: clear, viewport, blend, corner underlay.
    // Returns true if premultiplied alpha is active.
    bool beginRender()
    {
        if (m_width <= 0 || m_height <= 0) {
            return false;
        }

        // Context is already current: the window made it so before driving render().
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
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);

        m_rounded.drawCorners(m_width, m_height, g_config.scale);

        return usePremultiplied;
    }

    // Draw text vertex buffer using font shader
    void drawTextVerts(const std::vector<float> &         verts,
                       const Ui::Color &                  color,
                       Ui::Gl::FontRenderer::font_rec_t & fr) const
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_FRAMEBUFFER_SRGB);
        glUseProgram(fr.program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fr.atlas_tex);
        glUniform1i(fr.uTex, 0);
        glUniform1i(fr.uPremultiplied, 0);
        const auto fontProj = Ui::Gl::Util::orthoProjection(static_cast<fpx_t>(m_width), static_cast<fpx_t>(m_height));
        glUniformMatrix4fv(fr.uProjection, 1, GL_FALSE, fontProj.data());
        auto cf = color.toGLRGBA();
        glUniform4f(fr.uColor, cf.at(0), cf.at(1), cf.at(2), cf.at(3));

        glBindVertexArray(fr.vao);
        glBindBuffer(GL_ARRAY_BUFFER, fr.vbo);
        Ui::Gl::Util::drawTriangles(verts);
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

    static void endSvgDraw() { Ui::Gl::SvgRenderer::end(); }

    // -- Shared members --

    const Ui::Res::ResManager & m_resManager;
    Ui::task_fn_t               m_makeCurrent; // makes this renderer's window context current (for GL teardown)
    fpx_t                       m_width  = 0;
    fpx_t                       m_height = 0;
    Ui::Gl::FontRenderer        m_fontRenderer;
    Ui::Gl::SvgRenderer         m_svgRenderer;
    Ui::Gl::Rounded             m_rounded;
};

} // namespace Ui::Render::Popup
