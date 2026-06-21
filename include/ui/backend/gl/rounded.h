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

#include "ui/backend/gl/glutil.h"
#include "ui/color.h"
#include "ui/const.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/colorpair.h"
#include "ui/type.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

namespace Ui::Backend::Gl {

/**
 * @brief SDF-based rounded rectangle renderer with corner capture support
 *
 * Uses a fragment shader with signed distance field for pixel-perfect
 * rounded corners with smoothstep anti-aliasing. One instance per GL context.
 *
 * The SDF matches CSS border-radius semantics: each corner is independently
 * rounded by cutting the box with a circle in that corner's quadrant.
 *
 * The quad is exactly the requested bounds - no expand. Straight edges are
 * fully opaque (no AA). Corner arcs use inward smoothstep AA (from -aa to 0)
 * so the transition fits within the arc boundary. The arc is tangent to the
 * box at the junction, so the two edges meet flush.
 *
 * Two output modes controlled by bgColor.a:
 * - Opaque composite (bgColor.a > 0): blends fg/bg in shader, fully opaque output.
 *   Corner cutout pixels naturally show bgColor without needing discard.
 * - Premultiplied alpha (bgColor.a == 0): outputs vec4(color*alpha, alpha) for
 *   compositing over a capture layer or transparent window.
 *
 * Corner capture draws parent window pixels in popup corners for software
 * transparency on X11 without compositing. Draw order: drawCorners() first
 * as underlay, then begin()/draw()/end() with premultiplied alpha on top.
 *
 * Usage:
 *   m_rounded.setAlpha(hasAlpha);
 *   m_rounded.setCornerPixels(pixels, radii);  // optional, for software transparency
 *   m_rounded.drawCorners(viewW, viewH, scale); // underlay
 *   m_rounded.begin(viewW, viewH, scale);
 *   m_rounded.draw(bound, radii, fgColor, bgColor);
 *   m_rounded.end();
 */
class Rounded final {
public:
    Rounded() = default;

    ~Rounded() { cleanup(); }

    Rounded(const Rounded &)             = delete;
    Rounded(Rounded &&)                  = delete;
    Rounded & operator=(const Rounded &) = delete;
    Rounded & operator=(Rounded &&)      = delete;

    void setAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }

    [[nodiscard]] bool hasAlpha() const { return m_hasAlpha; }

    static fpx_t borderRadius(const Ui::Res::Type::border_t & b, id_t idx)
    {
        switch (idx) {
        case 0: return b.topLeft;
        case 1: return b.topRight;
        case 2: return b.bottomRight;
        case 3: return b.bottomLeft;
        default: return 0;
        }
    }

    /**
     * @brief Upload captured parent pixels as corner textures.
     *
     * Each corner buffer is radius*radius*4 bytes (RGBA, top-left origin).
     * Corners with zero radius or empty data are skipped.
     *
     * @param pixels  4 RGBA buffers [TL, TR, BR, BL]
     * @param radii   Per-corner radii in physical pixels
     */
    void setCornerPixels(std::array<std::vector<uint8_t>, 4> pixels, const Ui::Res::Type::border_t & radii)
    {
        cleanupCorners();
        m_cornerRadii = radii;

        for (id_t i = 0; i < 4; ++i) {
            const fpx_t radius = borderRadius(m_cornerRadii, i);
            if (radius <= 0 || pixels.at(i).empty()) {
                continue;
            }

            const auto texSize = static_cast<GLsizei>(radius);
            glGenTextures(1, &m_cornerTex.at(i));
            glBindTexture(GL_TEXTURE_2D, m_cornerTex.at(i));
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D,
                         0,
                         GL_RGBA8,
                         texSize,
                         texSize,
                         0,
                         GL_RGBA,
                         GL_UNSIGNED_BYTE,
                         pixels.at(i).data());
        }
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Update a single corner (0=TL, 1=TR, 2=BR, 3=BL) without touching others
    void updateCorner(id_t cornerIndex, const std::vector<uint8_t> & pixels, fpx_t radius)
    {
        if (cornerIndex >= 4) {
            return;
        }

        // Cleanup old texture for this corner
        if (m_cornerTex.at(cornerIndex) != 0) {
            glDeleteTextures(1, &m_cornerTex.at(cornerIndex));
            m_cornerTex.at(cornerIndex) = 0;
        }

        // Update radius
        switch (cornerIndex) {
        case 0: m_cornerRadii.topLeft = radius; break;
        case 1: m_cornerRadii.topRight = radius; break;
        case 2: m_cornerRadii.bottomRight = radius; break;
        case 3: m_cornerRadii.bottomLeft = radius; break;
        }

        if (radius <= 0 || pixels.empty()) {
            return;
        }

        const auto texSize = static_cast<GLsizei>(radius);
        glGenTextures(1, &m_cornerTex.at(cornerIndex));
        glBindTexture(GL_TEXTURE_2D, m_cornerTex.at(cornerIndex));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    [[nodiscard]] bool hasCornerPixels() const
    {
        for (id_t i = 0; i < 4; ++i) {
            if (m_cornerTex.at(i) != 0) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Draw captured parent pixels as underlay in popup corner areas.
     *
     * Call before begin()/draw()/end(). The SDF shader draws on top with
     * premultiplied alpha; its alpha=0 corner cutouts let the captured
     * background show through.
     */
    void drawCorners(fpx_t viewW, fpx_t viewH, fpx_t scale)
    {
        if (!hasCornerPixels()) {
            return;
        }

        if (m_cornerProgram == 0) {
            initCorner();
        }
        if (m_cornerProgram == 0) {
            return;
        }

        glUseProgram(m_cornerProgram);

        const fpx_t cssW = viewW / scale;
        const fpx_t cssH = viewH / scale;
        const auto  proj = Ui::Backend::Gl::Util::orthoProjection(cssW, cssH);
        glUniformMatrix4fv(m_cornerProjection, 1, GL_FALSE, proj.data());

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_cornerTexUniform, 0);

        glBindVertexArray(m_cornerVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

        const fpx_t cssRadii[4] = { m_cornerRadii.topLeft / scale,
                                    m_cornerRadii.topRight / scale,
                                    m_cornerRadii.bottomRight / scale,
                                    m_cornerRadii.bottomLeft / scale };

        for (id_t i = 0; i < 4; ++i) {
            if (m_cornerTex.at(i) == 0) {
                continue;
            }
            const fpx_t r = cssRadii[i];
            if (r <= 0) {
                continue;
            }

            glBindTexture(GL_TEXTURE_2D, m_cornerTex.at(i));

            fpx_t x0 = 0;
            fpx_t y0 = 0;
            fpx_t x1 = 0;
            fpx_t y1 = 0;
            switch (i) {
            case 0:
                x0 = 0;
                y0 = 0;
                x1 = r;
                y1 = r;
                break;
            case 1:
                x0 = cssW - r;
                y0 = 0;
                x1 = cssW;
                y1 = r;
                break;
            case 2:
                x0 = cssW - r;
                y0 = cssH - r;
                x1 = cssW;
                y1 = cssH;
                break;
            case 3:
                x0 = 0;
                y0 = cssH - r;
                x1 = r;
                y1 = cssH;
                break;
            default: continue;
            }

            // UV: (0,0) at top-left, (1,1) at bottom-right
            // Data is top-left origin; GL interprets as bottom-left -> standard UVs compensate
            const std::array<fpx_t, 24> verts = { x0, y0, 0, 0, x1, y0, 1, 0, x1, y1, 1, 1,
                                                  x0, y0, 0, 0, x1, y1, 1, 1, x0, y1, 0, 1 };
            Ui::Backend::Gl::Util::drawTriangles(verts, 6);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);
    }

    void begin(fpx_t viewW, fpx_t viewH, fpx_t scale)
    {
        if (m_program == 0) {
            init();
        }
        if (m_program == 0) {
            return;
        }

        m_aa = 1.0F / scale;

        glUseProgram(m_program);

        const fpx_t cssW = viewW / scale;
        const fpx_t cssH = viewH / scale;
        const auto  proj = Ui::Backend::Gl::Util::orthoProjection(cssW, cssH);
        glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, proj.data());
        glUniform1f(m_uAA, m_aa);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }

    void draw(const Ui::Res::Type::bound_t &      bound,
              const Ui::Res::Type::border_t &     radii,
              const Ui::Res::Type::color_pair_t & colors) const
    {
        if (m_program == 0) {
            return;
        }

        auto c  = colors.fg.toGLRGBA();
        auto bg = colors.bg.toGLRGBA();

        glUniform4f(m_uRect, bound.x, bound.y, bound.w, bound.h);
        glUniform4f(m_uRadii, radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft);
        glUniform4f(m_uColor, c.at(0), c.at(1), c.at(2), c.at(3));
        glUniform4f(m_uBgColor, bg.at(0), bg.at(1), bg.at(2), bg.at(3));
        glUniform1f(m_uSplitAngle, -1.0F); // single-color path

        // Exact bounds - no expand. Corner cutout pixels show bgColor via composite.
        // (x0,y0) (x1,y0)
        // (x0,y1) (x1,y1)
        const fpx_t x0 = bound.x;
        const fpx_t y0 = bound.y;
        const fpx_t x1 = bound.x + bound.w;
        const fpx_t y1 = bound.y + bound.h;

        const std::array<fpx_t, 12> verts = { x0, y0, x1, y0, x1, y1, x1, y1, x0, y1, x0, y0 };
        Ui::Backend::Gl::Util::drawTriangles(verts, 6);
    }

    /**
     * @brief Draw a rounded rect filled with two colors split along an angled line.
     *
     * Split semantics (screen Y-down, normal = (sin t, cos t)):
     *   splitAngleDeg =  0 -> horizontal split, colorR on bottom
     *   splitAngleDeg = 45 -> "/" line,         colorR on lower-right (left half = colorL)
     *   splitAngleDeg = 90 -> vertical split,   colorR on right
     *
     * @param colorL primary color (occupies the half away from the split normal)
     * @param colorR secondary color (occupies the half the split normal points toward)
     * @param bgColor opaque composite background (a==0 -> premultiplied output)
     */
    void drawSplit(const Ui::Res::Type::bound_t &  bound,
                   const Ui::Res::Type::border_t & radii,
                   const Ui::Color &               colorL,
                   const Ui::Color &               colorR,
                   fpx_t                           splitAngleDeg,
                   const Ui::Color &               bgColor) const
    {
        if (m_program == 0) {
            return;
        }

        auto cl = colorL.toGLRGBA();
        auto cr = colorR.toGLRGBA();
        auto bg = bgColor.toGLRGBA();

        glUniform4f(m_uRect, bound.x, bound.y, bound.w, bound.h);
        glUniform4f(m_uRadii, radii.topLeft, radii.topRight, radii.bottomRight, radii.bottomLeft);
        glUniform4f(m_uColor, cl.at(0), cl.at(1), cl.at(2), cl.at(3));
        glUniform4f(m_uColor2, cr.at(0), cr.at(1), cr.at(2), cr.at(3));
        glUniform4f(m_uBgColor, bg.at(0), bg.at(1), bg.at(2), bg.at(3));
        glUniform1f(m_uSplitAngle, splitAngleDeg * PI / 180.0F);

        const fpx_t x0 = bound.x;
        const fpx_t y0 = bound.y;
        const fpx_t x1 = bound.x + bound.w;
        const fpx_t y1 = bound.y + bound.h;

        const std::array<fpx_t, 12> verts = { x0, y0, x1, y0, x1, y1, x1, y1, x0, y1, x0, y0 };
        Ui::Backend::Gl::Util::drawTriangles(verts, 6);
    }

    static void end()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    void cleanup()
    {
        cleanupCorners();
        Ui::Backend::Gl::Util::deleteProgram(m_cornerProgram);
        Ui::Backend::Gl::Util::deleteVertexArray(m_cornerVao);
        Ui::Backend::Gl::Util::deleteProgram(m_program);
        Ui::Backend::Gl::Util::deleteBuffer(m_vbo);
        Ui::Backend::Gl::Util::deleteVertexArray(m_vao);
    }

private:
    void init()
    {
        const char * vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec2 a_position;
            uniform mat4 u_projection;
            out vec2 v_pos;
            void main() {
                gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
                v_pos = a_position;
            }
        )";

        // Exact-bounds rounded rect SDF. Straight edges are fully opaque (no AA).
        // Corner arcs use inward smoothstep (-u_aa..0): the AA transition fits
        // inside the arc boundary.
        //
        // Two output modes based on u_bgColor.a:
        // - Opaque (a > 0): composite fg/bg in shader, always fully opaque output
        // - Premultiplied (a == 0): output vec4(color*alpha, alpha) for layer compositing
        const char * fragSrc = R"(
            #version 330 core
            in vec2 v_pos;
            out vec4 fragColor;
            uniform vec4 u_rect;
            uniform vec4 u_radii;
            uniform vec4 u_color;
            uniform vec4 u_color2;      // secondary color for split mode
            uniform float u_splitAngle; // split line angle in radians; <0 = single color
            uniform vec4 u_bgColor;
            uniform float u_aa;

            void main() {
                vec2 center = u_rect.xy + u_rect.zw * 0.5;
                vec2 halfSize = u_rect.zw * 0.5;
                vec2 p = v_pos - center;

                // Box distance (positive = outside)
                vec2 d = abs(p) - halfSize;
                float boxDist = max(d.x, d.y);

                float alpha = 1.0;

                // Corner arc cuts with inward AA
                // TL: radii.x
                if (u_radii.x > 0.0) {
                    vec2 cc = vec2(-halfSize.x + u_radii.x, -halfSize.y + u_radii.x);
                    if (p.x < cc.x && p.y < cc.y) {
                        float cd = length(p - cc) - u_radii.x;
                        alpha = min(alpha, 1.0 - smoothstep(-u_aa, u_aa, cd));
                    }
                }
                // TR: radii.y
                if (u_radii.y > 0.0) {
                    vec2 cc = vec2(halfSize.x - u_radii.y, -halfSize.y + u_radii.y);
                    if (p.x > cc.x && p.y < cc.y) {
                        float cd = length(p - cc) - u_radii.y;
                        alpha = min(alpha, 1.0 - smoothstep(-u_aa, u_aa, cd));
                    }
                }
                // BR: radii.z
                if (u_radii.z > 0.0) {
                    vec2 cc = vec2(halfSize.x - u_radii.z, halfSize.y - u_radii.z);
                    if (p.x > cc.x && p.y > cc.y) {
                        float cd = length(p - cc) - u_radii.z;
                        alpha = min(alpha, 1.0 - smoothstep(-u_aa, u_aa, cd));
                    }
                }
                // BL: radii.w
                if (u_radii.w > 0.0) {
                    vec2 cc = vec2(-halfSize.x + u_radii.w, halfSize.y - u_radii.w);
                    if (p.x < cc.x && p.y > cc.y) {
                        float cd = length(p - cc) - u_radii.w;
                        alpha = min(alpha, 1.0 - smoothstep(-u_aa, u_aa, cd));
                    }
                }

                // Pick the surface color: when a split angle is set, mix two
                // colors along a line through the rect center.
                vec4 surface = u_color;
                if (u_splitAngle >= 0.0) {
                    // Normal points toward the u_color2 half. Convention
                    // (screen Y-down ortho):
                    //   angle = 0     -> horizontal split, color2 on bottom
                    //   angle = pi/4  -> "/" line,         color2 on lower-right
                    //   angle = pi/2  -> vertical split,   color2 on right
                    vec2 n = vec2(sin(u_splitAngle), cos(u_splitAngle));
                    float side = dot(p, n);
                    float t    = smoothstep(-u_aa, u_aa, side);
                    surface    = mix(u_color, u_color2, t);
                }

                if (u_bgColor.a > 0.0) {
                    // Opaque composite - blend against background, fully opaque output
                    vec3 blended = mix(u_bgColor.rgb, surface.rgb, alpha);
                    fragColor = vec4(blended, surface.a);
                } else {
                    // Premultiplied alpha - for compositing over capture layer
                    float a = surface.a * alpha;
                    fragColor = vec4(surface.rgb * a, a);
                }
            }
        )";

        const GLuint vert = Ui::Backend::Gl::Util::compileShader(GL_VERTEX_SHADER,
                                                                 vertSrc,
                                                                 "[Rounded] Vertex shader error: ");
        if (vert == 0) {
            return;
        }

        const GLuint frag = Ui::Backend::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                                 fragSrc,
                                                                 "[Rounded] Fragment shader error: ");
        if (frag == 0) {
            glDeleteShader(vert);
            return;
        }

        m_program = Ui::Backend::Gl::Util::linkProgram(vert, frag, "[Rounded] Link error: ");
        if (m_program == 0) {
            return;
        }

        m_uProjection = glGetUniformLocation(m_program, "u_projection");
        m_uRect       = glGetUniformLocation(m_program, "u_rect");
        m_uRadii      = glGetUniformLocation(m_program, "u_radii");
        m_uColor      = glGetUniformLocation(m_program, "u_color");
        m_uColor2     = glGetUniformLocation(m_program, "u_color2");
        m_uSplitAngle = glGetUniformLocation(m_program, "u_splitAngle");
        m_uBgColor    = glGetUniformLocation(m_program, "u_bgColor");
        m_uAA         = glGetUniformLocation(m_program, "u_aa");

        glGenVertexArrays(1, &m_vao);
        if (m_vbo == 0) {
            glGenBuffers(1, &m_vbo);
        }

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void initCorner()
    {
        const char * vertSrc = R"(
            #version 330 core
            layout(location = 0) in vec2 a_position;
            layout(location = 1) in vec2 a_texcoord;
            out vec2 v_texcoord;
            uniform mat4 u_projection;
            void main() {
                gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
                v_texcoord = a_texcoord;
            }
        )";

        const char * fragSrc = R"(
            #version 330 core
            in vec2 v_texcoord;
            out vec4 fragColor;
            uniform sampler2D u_texture;
            void main() {
                fragColor = texture(u_texture, v_texcoord);
            }
        )";

        const GLuint vert = Ui::Backend::Gl::Util::compileShader(GL_VERTEX_SHADER,
                                                                 vertSrc,
                                                                 "[Rounded:Corner] Vert error: ");
        if (vert == 0) {
            return;
        }

        const GLuint frag = Ui::Backend::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                                 fragSrc,
                                                                 "[Rounded:Corner] Frag error: ");
        if (frag == 0) {
            glDeleteShader(vert);
            return;
        }

        m_cornerProgram = Ui::Backend::Gl::Util::linkProgram(vert, frag, "[Rounded:Corner] Link error: ");
        if (m_cornerProgram == 0) {
            return;
        }

        m_cornerProjection = glGetUniformLocation(m_cornerProgram, "u_projection");
        m_cornerTexUniform = glGetUniformLocation(m_cornerProgram, "u_texture");

        GLuint cornerVbo = 0;
        Ui::Backend::Gl::Util::createPosUvVao(m_cornerVao, cornerVbo);
        if (m_vbo == 0) {
            m_vbo = cornerVbo;
        } else {
            Ui::Backend::Gl::Util::deleteBuffer(cornerVbo);
        }
    }

    void cleanupCorners()
    {
        for (auto & tex : m_cornerTex) {
            Ui::Backend::Gl::Util::deleteTexture(tex);
        }
    }

    // Rounded rect shader
    GLuint m_program     = 0;
    GLuint m_vao         = 0;
    GLuint m_vbo         = 0;
    GLint  m_uProjection = -1;
    GLint  m_uRect       = -1;
    GLint  m_uRadii      = -1;
    GLint  m_uColor      = -1;
    GLint  m_uColor2     = -1;
    GLint  m_uSplitAngle = -1;
    GLint  m_uBgColor    = -1;
    GLint  m_uAA         = -1;
    fpx_t  m_aa          = 0.5F;

    // Corner texture shader
    GLuint m_cornerProgram    = 0;
    GLuint m_cornerVao        = 0;
    GLint  m_cornerProjection = -1;
    GLint  m_cornerTexUniform = -1;

    // Corner textures and radii (physical pixels)
    std::array<GLuint, 4>   m_cornerTex = { 0, 0, 0, 0 };
    Ui::Res::Type::border_t m_cornerRadii {};

    bool m_hasAlpha = false;
};

} // namespace Ui::Backend::Gl
