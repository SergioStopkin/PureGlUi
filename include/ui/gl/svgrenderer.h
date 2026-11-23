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
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/cachekey.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/gl/svgdocuments.h"
#include "ui/gl/svgtexture.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ui::Gl {

// SVG icons drawn in one window's GL context: each document SvgDocuments holds, rasterized
// once per size and kept as a texture (svg_texture_t), drawn plain, tinted, scaled or over
// a soft shadow
class SvgRenderer final : private Common::NonCopyable {
public:
    SvgRenderer() = default;

    ~SvgRenderer()
    {
        for (auto & pair : m_textures) {
            Ui::Gl::Util::deleteTexture(pair.second.textureId);
        }
        for (auto & pair : m_shadowTextures) {
            Ui::Gl::Util::deleteTexture(pair.second.textureId);
        }
        cleanupGl();
    }

    // Control whether textures are uploaded premultiplied for composited popups
    static void setUploadPremultiplied(bool v) { s_isUploadPremultiplied = v; }

    void begin(fpx_t viewWidth, fpx_t viewHeight, fpx_t scale)
    {
        initGl();
        if (m_program == 0) {
            return;
        }

        glUseProgram(m_program);

        const fpx_t cssWidth   = viewWidth / scale;
        const fpx_t cssHeight  = viewHeight / scale;
        const auto  projection = Ui::Gl::Util::orthoProjection(cssWidth, cssHeight);
        glUniformMatrix4fv(m_uProjection, 1, GL_FALSE, projection.data());

        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_uTex, 0);
        glUniform1i(m_uTint, 0);

        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    }

    static void end()
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    /**
     * @brief Get or render SVG to texture at specified size
     * @param svgId Document ID from SvgDocuments' load functions
     * @param width Target width in CSS pixels
     * @param height Target height in CSS pixels (0 = preserve aspect ratio)
     * @return Texture info, check .valid for success
     */
    svg_texture_t texture(const std::string & svgId, fpx_t width, fpx_t height = 0)
    {
        float svgW = 0;
        float svgH = 0;
        if (!Ui::Gl::SvgDocuments::intrinsicSizeIfCached(svgId, svgW, svgH)) {
            std::cerr << "[SvgRenderer] SVG not found: " << svgId << std::endl;
            return {};
        }

        // Calculate actual render size with scale factor
        int renderWidth  = toPhysFloor(width);
        int renderHeight = height > 0 ? toPhysFloor(height) : 0;

        // If height is 0, calculate from aspect ratio
        if (renderHeight == 0 && svgW > 0) {
            const float aspect = svgH / svgW;
            renderHeight       = static_cast<int>(renderWidth * aspect);
        }

        if (renderWidth <= 0 || renderHeight <= 0) {
            renderWidth = renderHeight = toPhysFloor(32);
        }

        // Check cache
        const cache_key_t key { .svgId = svgId, .width = renderWidth, .height = renderHeight };
        auto              cacheIt = m_textures.find(key);
        if (cacheIt != m_textures.end()) {
            return cacheIt->second;
        }

        // Render to bitmap via Cairo, the ink rect centered within the target where the
        // document has transparent padding worth cropping
        const std::vector<uint8_t> rgba = Ui::Gl::SvgDocuments::rgbaOf(svgId,
                                                                       renderWidth,
                                                                       renderHeight,
                                                                       s_isUploadPremultiplied);
        if (rgba.empty()) {
            std::cerr << "[SvgRenderer] Failed to render SVG: " << svgId << std::endl;
            return {};
        }

        // Drain any stale GL errors from earlier operations so the check below is
        // attributed to our texture creation, not to an unrelated earlier call.
        while (glGetError() != GL_NO_ERROR) { }

        // Create OpenGL texture
        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, renderWidth, renderHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        const GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "[SvgRenderer] GL error creating texture: " << err << std::endl;
            glDeleteTextures(1, &tex);
            return {};
        }

        glBindTexture(GL_TEXTURE_2D, 0);

        // Cache and return
        svg_texture_t result;
        result.textureId = tex;
        result.width     = renderWidth;
        result.height    = renderHeight;
        result.valid     = true;

        m_textures[key] = result;

        return result;
    }

    // Get or create a shadow texture: same shape as the SVG but with RGB=white, A=original.
    // This allows GL_MODULATE with vertex color = shadow color to produce correct shadow RGB.
    svg_texture_t shadowTexture(const std::string & svgId, fpx_t width, fpx_t height = 0)
    {
        auto baseTex = texture(svgId, width, height);
        if (!baseTex.valid) {
            return {};
        }

        const cache_key_t key { .svgId = svgId, .width = baseTex.width, .height = baseTex.height };
        auto              cacheIt = m_shadowTextures.find(key);
        if (cacheIt != m_shadowTextures.end()) {
            return cacheIt->second;
        }

        // Read back pixels from the base texture via temporary FBO
        const int            w = baseTex.width;
        const int            h = baseTex.height;
        std::vector<uint8_t> rgba(w * h * 4);

        GLuint fbo = 0;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, baseTex.textureId, 0);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);

        // Build alpha-only shape mask.
        // Premultiplied mode: set RGB = A (premultiplied white: R=G=B=A is valid).
        // Straight alpha mode: set RGB = 255 (full white, alpha is the shape).
        const int pixels = w * h;
        int       i      = 0;
        while (i < pixels) {
            const int     idx = i * 4;
            const uint8_t a   = rgba.at(idx + 3);
            if (s_isUploadPremultiplied) {
                rgba.at(idx)     = a; // R = A (premultiplied white)
                rgba.at(idx + 1) = a; // G = A
                rgba.at(idx + 2) = a; // B = A
            } else {
                rgba.at(idx)     = 255; // R
                rgba.at(idx + 1) = 255; // G
                rgba.at(idx + 2) = 255; // B
            }
            ++i;
        }

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, 0);

        svg_texture_t result;
        result.textureId = tex;
        result.width     = w;
        result.height    = h;
        result.valid     = true;

        m_shadowTextures[key] = result;
        return result;
    }

    /**
     * @brief Draw SVG at position (in current GL coordinate system)
     */
    void draw(const std::string & svgId, const Ui::Res::Type::bound_t & bound)
    {
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW = bound.w;
        const fpx_t drawH = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);

        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform4f(m_uColor, 1.0F, 1.0F, 1.0F, 1.0F);
        glUniform1i(m_uTint, 0);
        drawQuad(bound.x, bound.y, drawW, drawH);
    }

    /**
     * @brief Draw SVG with color tint
     */
    void drawTinted(const std::string & svgId, const Ui::Res::Type::bound_t & bound, const Ui::Color & color)
    {
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW = bound.w;
        const fpx_t drawH = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);

        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform1i(m_uTint, 1);
        auto c = color.toGLRGBA();
        glUniform4f(m_uColor, c.at(0), c.at(1), c.at(2), c.at(3));
        drawQuad(bound.x, bound.y, drawW, drawH);
    }

    /**
     * @brief Draw tinted SVG with soft shadow (for tinted icon hover state)
     */
    void drawTintedWithShadow(const std::string &            svgId,
                              const Ui::Res::Type::bound_t & bound,
                              const Ui::Color &              color,
                              float                          shadowOffsetX,
                              float                          shadowOffsetY,
                              float                          blurRadius,
                              const Ui::Color &              shadowColor,
                              float                          shadowOpacity)
    {
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW = bound.w;
        const fpx_t drawH = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);

        drawShadowLayers(svgId,
                         bound,
                         drawW,
                         drawH,
                         shadowOffsetX,
                         shadowOffsetY,
                         blurRadius,
                         shadowColor,
                         shadowOpacity);

        // Draw main icon with tint
        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform1i(m_uTint, 1);
        auto c = color.toGLRGBA();
        glUniform4f(m_uColor, c.at(0), c.at(1), c.at(2), c.at(3));
        drawQuad(bound.x, bound.y, drawW, drawH);
    }

    /**
     * @brief Draw tinted SVG scaled (for tinted icon active/pressed state)
     */
    void drawTintedScaled(const std::string &            svgId,
                          const Ui::Res::Type::bound_t & bound,
                          const Ui::Color &              color,
                          float                          scale = 0.9F)
    {
        drawScaled(svgId, bound, scale, true, color);
    }

    /**
     * @brief Draw SVG with soft shadow effect (for hover state)
     */
    void drawWithShadow(const std::string &            svgId,
                        const Ui::Res::Type::bound_t & bound,
                        float                          shadowOffsetX,
                        float                          shadowOffsetY,
                        float                          blurRadius,
                        const Ui::Color &              shadowColor,
                        float                          shadowOpacity)
    {
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW = bound.w;
        const fpx_t drawH = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);

        drawShadowLayers(svgId,
                         bound,
                         drawW,
                         drawH,
                         shadowOffsetX,
                         shadowOffsetY,
                         blurRadius,
                         shadowColor,
                         shadowOpacity);

        // Draw main icon with original texture
        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform4f(m_uColor, 1.0F, 1.0F, 1.0F, 1.0F);
        glUniform1i(m_uTint, 0);
        drawQuad(bound.x, bound.y, drawW, drawH);
    }

    /**
     * @brief Draw SVG scaled/resized (for active/pressed state)
     */
    void drawScaled(const std::string & svgId, const Ui::Res::Type::bound_t & bound, float scale = 0.9F)
    {
        drawScaled(svgId, bound, scale, false, {});
    }

private:
    // In their premultiplied ARGB form, for an ARGB composited popup blending premultiplied; per thread
    inline static thread_local bool s_isUploadPremultiplied = false;

    std::unordered_map<cache_key_t, svg_texture_t> m_textures;       // rendered textures by id and size
    std::unordered_map<cache_key_t, svg_texture_t> m_shadowTextures; // alpha-only masks: RGB=white, A=shape

    // Shader resources for GL 3.3 core rendering (replaces fixed-function pipeline)
    GLuint m_program     = 0;
    GLuint m_vao         = 0;
    GLuint m_vbo         = 0;
    GLint  m_uProjection = -1;
    GLint  m_uColor      = -1;
    GLint  m_uTex        = -1;
    GLint  m_uTint       = -1;

    void initGl()
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
            uniform vec4 u_color;
            uniform int u_tint;
            void main() {
                vec4 texel = texture(u_tex, vUV);
                if (u_tint != 0) {
                    fragColor = vec4(u_color.rgb, u_color.a * texel.a);
                } else {
                    fragColor = u_color * texel;
                }
            }
        )GLSL";

        const GLuint vert = Ui::Gl::Util::compileShader(GL_VERTEX_SHADER, vertexSource, "[SVG] Vertex shader error: ");
        const GLuint frag = Ui::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                        fragmentSource,
                                                        "[SVG] Fragment shader error: ");
        if (vert == 0 || frag == 0) {
            if (vert != 0) {
                glDeleteShader(vert);
            }
            if (frag != 0) {
                glDeleteShader(frag);
            }
            return;
        }

        m_program = Ui::Gl::Util::linkProgram(vert, frag, "[SVG] Shader link error: ");
        if (m_program == 0) {
            return;
        }

        m_uProjection = glGetUniformLocation(m_program, "u_projection");
        m_uColor      = glGetUniformLocation(m_program, "u_color");
        m_uTex        = glGetUniformLocation(m_program, "u_tex");
        m_uTint       = glGetUniformLocation(m_program, "u_tint");

        Ui::Gl::Util::createPosUvVao(m_vao, m_vbo);
    }

    void cleanupGl()
    {
        Ui::Gl::Util::deleteProgram(m_program);
        Ui::Gl::Util::deleteBuffer(m_vbo);
        Ui::Gl::Util::deleteVertexArray(m_vao);
    }

    static void drawQuad(fpx_t x, fpx_t y, fpx_t width, fpx_t height)
    {
        Ui::Gl::Util::drawTriangles(Ui::Gl::Util::quadVertices(x, y, width, height), 6);
    }

    // Shared body of the public drawScaled/drawTintedScaled (they differ only
    // in the tint uniforms). color is ignored when isTinted is false.
    //
    // Quality: the scaled icon gets its own raster at the scaled size (its own
    // texture-cache entry, kept warm by the render loop) instead of a
    // GPU-downscale of the full-size texture, and the quad is drawn at EXACTLY
    // the raster's physical size, centered on the physical-pixel grid. The
    // naive quad (bound.w * scale CSS) covers a fractional pixel count (e.g.
    // 30.96 px over a 30 px raster), so the texture gets resampled and the
    // pressed (iconActiveScale) icon blurs.
    void drawScaled(const std::string &            svgId,
                    const Ui::Res::Type::bound_t & bound,
                    float                          scale,
                    bool                           isTinted,
                    const Ui::Color &              color)
    {
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w * scale, bound.h * scale);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW = bound.w;
        const fpx_t drawH = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);

        // tex.width/height are the integral physical raster dims; convert back
        // to CSS so the quad covers them 1:1, and snap the centered origin to
        // whole physical pixels.
        const fpx_t scaledW = toCss(static_cast<fpx_t>(tex.width));
        const fpx_t scaledH = toCss(static_cast<fpx_t>(tex.height));
        const fpx_t quadX   = toCss(std::round(toPhys(bound.x + ((drawW - scaledW) / 2.0F))));
        const fpx_t quadY   = toCss(std::round(toPhys(bound.y + ((drawH - scaledH) / 2.0F))));

        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        if (isTinted) {
            glUniform1i(m_uTint, 1);
            auto c = color.toGLRGBA();
            glUniform4f(m_uColor, c.at(0), c.at(1), c.at(2), c.at(3));
        } else {
            glUniform4f(m_uColor, 1.0F, 1.0F, 1.0F, 1.0F);
            glUniform1i(m_uTint, 0);
        }
        drawQuad(quadX, quadY, scaledW, scaledH);
    }

    void drawShadowLayers(const std::string &            svgId,
                          const Ui::Res::Type::bound_t & bound,
                          fpx_t                          drawW,
                          fpx_t                          drawH,
                          fpx_t                          shadowOffsetX,
                          fpx_t                          shadowOffsetY,
                          fpx_t                          blurRadius,
                          const Ui::Color &              shadowColor,
                          fpx_t                          shadowOpacity)
    {
        auto sTex = shadowTexture(svgId, bound.w, bound.h);
        if (!sTex.valid) {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, sTex.textureId);
        glUniform1i(m_uTint, 0);

        if (s_isUploadPremultiplied) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        auto        sc = shadowColor.toGLRGBA();
        const float sr = sc.at(0);
        const float sg = sc.at(1);
        const float sb = sc.at(2);

        Ui::Gl::Util::forEachShadowLayer(
            shadowOffsetX,
            shadowOffsetY,
            blurRadius,
            shadowOpacity,
            [&](fpx_t ox, fpx_t oy, fpx_t expand, fpx_t alpha) {
                if (s_isUploadPremultiplied) {
                    glUniform4f(m_uColor, sr * alpha, sg * alpha, sb * alpha, alpha);
                } else {
                    glUniform4f(m_uColor, sr, sg, sb, alpha);
                }
                drawQuad(bound.x + ox - expand, bound.y + oy - expand, drawW + (expand * 2), drawH + (expand * 2));
            });

        if (s_isUploadPremultiplied) {
            Ui::Gl::Util::blendStraightAlpha();
        }
    }
};

} // namespace Ui::Gl
