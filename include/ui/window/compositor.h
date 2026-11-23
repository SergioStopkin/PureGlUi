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
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/res/type/bound.h"
#include "ui/window/compositetexture.h"
#include "ui/window/geometry.h"

namespace Ui::Window {

// Draws a window captured into a CompositeTexture onto the main window's back buffer -
// a content surface or a popup standing offscreen while composited. Its shader lives in
// the main window's GL context, made there on first use
class Compositor final : private Common::NonCopyable {
public:
    explicit Compositor(const Geometry & geometry)
        : m_geometry(geometry)
    {
    }

    /**
     * @brief Upload (if dirty) and blit a captured surface texture onto the main
     * back buffer at `dest`, using the shared composite shader. Texture handle +
     * dirty flag are the texture's own so any content surface can reuse this. `blend`
     * is true for translucent popups, false for opaque content viewports.
     */
    void draw(const Ui::Res::Type::bound_t & dest, bool blend, CompositeTexture & texture)
    {
        init();
        if (m_program == 0) {
            return;
        }

        if (texture.texture == 0) {
            glGenTextures(1, &texture.texture);
            glBindTexture(GL_TEXTURE_2D, texture.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        if (texture.dirty) {
            glBindTexture(GL_TEXTURE_2D, texture.texture);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA,
                         texture.w,
                         texture.h,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         texture.pixels.data());
            texture.dirty = false;
        }

        glUseProgram(m_program);
        const auto projection = Ui::Gl::Util::orthoProjection(m_geometry.width(), m_geometry.height());
        glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, projection.data());
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_uTex, 0);

        if (blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBindTexture(GL_TEXTURE_2D, texture.texture);

        Ui::Gl::Util::drawTriangles(Ui::Gl::Util::quadVerticesFlipY(dest.x, dest.y, dest.w, dest.h), 6);

        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    // Its shader let go of, in the main window's context
    void cleanup()
    {
        Ui::Gl::Util::deleteProgram(m_program);
        Ui::Gl::Util::deleteBuffer(m_vbo);
        Ui::Gl::Util::deleteVertexArray(m_vao);
    }

private:
    const Geometry & m_geometry;
    GLuint           m_program     = 0;
    GLuint           m_vao         = 0;
    GLuint           m_vbo         = 0;
    GLint            m_uProjection = -1;
    GLint            m_uTex        = -1;

    void init()
    {
        if (m_program != 0) {
            return;
        }

        const char * vertexSource = R"GLSL(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aUV;
            out vec2 vUV;
            uniform mat4 u_projection;
            void main() {
                gl_Position = u_projection * vec4(aPos, 0.0, 1.0);
                vUV = aUV;
            }
        )GLSL";

        const char * fragmentSource = R"GLSL(
            #version 330 core
            in vec2 vUV;
            out vec4 fragColor;
            uniform sampler2D u_tex;
            void main() {
                fragColor = texture(u_tex, vUV);
            }
        )GLSL";

        const GLuint vert = Ui::Gl::Util::compileShader(GL_VERTEX_SHADER, vertexSource, "[Composite] Vertex error: ");
        const GLuint frag = Ui::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                        fragmentSource,
                                                        "[Composite] Fragment error: ");
        if (vert == 0 || frag == 0) {
            if (vert != 0) {
                glDeleteShader(vert);
            }
            if (frag != 0) {
                glDeleteShader(frag);
            }
            return;
        }

        m_program = Ui::Gl::Util::linkProgram(vert, frag, "[Composite] Link error: ");
        if (m_program == 0) {
            return;
        }

        m_uProjection = glGetUniformLocation(m_program, "u_projection");
        m_uTex        = glGetUniformLocation(m_program, "u_tex");

        Ui::Gl::Util::createPosUvVao(m_vao, m_vbo);
    }
};

} // namespace Ui::Window
