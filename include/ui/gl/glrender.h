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
#include "common/unicode.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/fontrenderer.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/gl/rounded.h"
#include "ui/gl/svgrenderer.h"
#include "ui/gl/textalign.h"
#include "ui/interface/irender.h"
#include "ui/render/shadow.h"
#include "ui/type.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui::Gl {

// GL implementation of the framework's draw sink (Ui::IRender). Wraps the GL
// backend classes (Rounded SDF rects, SvgRenderer, FontRenderer) plus a flat
// shader for the loading-bar arrow. Domain-blind: it knows nothing about menus,
// tabs, or ResManager - callers resolve all theme/layout values to plain args.
//
// Batching: rounded rects and images each need a begin()/end() GL setup, so the
// renderer lazily switches "mode" as primitive kinds change (callers issue them
// grouped, so transitions are rare). Text is buffered and flushed font-batched
// in endFrame() so it always composites above non-text.
class GlRender final : public Ui::IRender, private Common::NonCopyable {
public:
    // makeCurrent makes the host's GL context current (for font GL teardown).
    GlRender(Ui::task_fn_t makeCurrent, const std::string & fontDir)
        : m_makeCurrent(std::move(makeCurrent))
        , m_fontRenderer(std::make_unique<Ui::Gl::FontRenderer>(m_makeCurrent, fontDir))
    {
    }

    ~GlRender() override
    {
        // Make our window's GL context current before tearing down GL: the flat
        // shader below and the Rounded/Svg/Font members must glDelete in THIS
        // context, not whatever is current at destruction time. Contexts are not
        // shared, so deleting in the wrong one corrupts another window's objects.
        if (m_makeCurrent) {
            m_makeCurrent();
        }
        Ui::Gl::Util::deleteProgram(m_flatProgram);
        Ui::Gl::Util::deleteBuffer(m_flatVbo);
        Ui::Gl::Util::deleteVertexArray(m_flatVao);
    }

    // Concrete accessor used by popup/dialog renderers that share this font
    // renderer. Not part of IRender - retired when popups route through IRender.
    [[nodiscard]] Ui::Gl::FontRenderer * fontRenderer() { return m_fontRenderer.get(); }

    void beginFrame(Ui::fpx_t width, Ui::fpx_t height) override
    {
        m_width  = width;
        m_height = height;
        m_scale  = g_config.scale;

        glViewport(0, 0, static_cast<int>(width), static_cast<int>(height));

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        glBindVertexArray(0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        m_mode = Mode::None;
        m_textBatch.clear();
    }

    void fillRect(const Ui::Res::Type::bound_t &      bound,
                  const Ui::Res::Type::border_t &     radii,
                  const Ui::Res::Type::color_pair_t & colors,
                  const Ui::Render::shadow_t &        shadow) override
    {
        ensureMode(Mode::Rect);
        if (shadow.isVisible()) {
            drawShadow(bound, radii, shadow);
        }
        m_rounded.draw(bound, radii, colors);
    }

    void drawText(Ui::font_handle_t              font,
                  std::string_view               text,
                  const Ui::Res::Type::bound_t & pos,
                  const Ui::Color &              color,
                  Ui::Res::Type::AlignH          alignH,
                  Ui::Res::Type::AlignV          alignV,
                  Ui::fpx_t                      minPadH) override
    {
        m_textBatch.emplace_back(text_item { font, std::string(text), pos, color, alignH, alignV, minPadH });
    }

    void drawImage(std::string_view               src,
                   const Ui::Res::Type::bound_t & bound,
                   const Ui::Res::Type::border_t & /*radii*/,
                   const Ui::Color &            tint,
                   Ui::fpx_t                    scale,
                   const Ui::Render::shadow_t & shadow,
                   bool                         isFilled) override
    {
        const bool        isTinted = tint.a() > 0;
        const std::string key      = resolveImageKey(src, isTinted && isFilled);
        if (key.empty()) {
            return;
        }

        ensureMode(Mode::Image);

        // Any non-identity scale takes the scaled path - the press effect
        // (iconActiveScale) SHRINKS the icon (e.g. 0.86), so `scale > 1.0F`
        // would silently drop it.
        const bool isScaled  = scale != 1.0F;
        const bool hasShadow = shadow.isVisible();

        if (isTinted) {
            if (isScaled) {
                m_svgRenderer.drawTintedScaled(key, bound, tint, scale);
            } else if (hasShadow) {
                m_svgRenderer.drawTintedWithShadow(key,
                                                   bound,
                                                   tint,
                                                   shadow.offsetX,
                                                   shadow.offsetY,
                                                   shadow.blur,
                                                   shadow.color,
                                                   shadow.opacity);
            } else {
                m_svgRenderer.drawTinted(key, bound, tint);
            }
        } else if (isScaled) {
            m_svgRenderer.drawScaled(key, bound, scale);
        } else if (hasShadow) {
            m_svgRenderer
            .drawWithShadow(key, bound, shadow.offsetX, shadow.offsetY, shadow.blur, shadow.color, shadow.opacity);
        } else {
            m_svgRenderer.draw(key, bound);
        }
    }

    // Pre-rasterize the scaled variant of an image so a later scaled draw (the
    // button press effect, iconActiveScale) hits the texture cache instead of
    // rasterizing mid-frame. Cheap once cached - a map hit. Requires a current
    // GL context (called from the render loop).
    void warmImage(std::string_view               src,
                   const Ui::Res::Type::bound_t & bound,
                   const Ui::Color &              tint,
                   Ui::fpx_t                      scale,
                   bool                           isFilled) override
    {
        const std::string key = resolveImageKey(src, tint.a() > 0 && isFilled);
        if (key.empty()) {
            return;
        }
        m_svgRenderer.texture(key, bound.w * scale, bound.h * scale);
    }

    void drawTriangle(Ui::fpx_t         x0,
                      Ui::fpx_t         y0,
                      Ui::fpx_t         x1,
                      Ui::fpx_t         y1,
                      Ui::fpx_t         x2,
                      Ui::fpx_t         y2,
                      const Ui::Color & color) override
    {
        endCurrentMode(); // close any rounded/image batch before the flat shader
        initFlatShader();
        if (m_flatProgram == 0) {
            return;
        }

        glUseProgram(m_flatProgram);
        const auto projection = Ui::Gl::Util::orthoProjection(m_width / m_scale, m_height / m_scale);
        glUniformMatrix4fv(m_flatUProjection, 1, GL_FALSE, projection.data());

        auto lf = color.toGLRGBA();
        glUniform4f(m_flatUColor, lf.at(0), lf.at(1), lf.at(2), lf.at(3));

        glEnable(GL_BLEND);
        glBindVertexArray(m_flatVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_flatVbo);

        const std::array<float, 6> verts = { x0, y0, x1, y1, x2, y2 };
        Ui::Gl::Util::drawTriangles(verts, 3);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);

        m_mode = Mode::None; // next rect/image re-begins its batch
    }

    void endFrame() override
    {
        endCurrentMode();
        flushText();
    }

    [[nodiscard]] Ui::fpx_t textWidth(Ui::font_handle_t font, std::wstring_view text) override
    {
        return m_fontRenderer->textWidth(font, text);
    }

    Ui::font_handle_t createFont(const Ui::Res::Type::font_t & font) override
    {
        return m_fontRenderer->createFont(font);
    }

private:
    enum class Mode : uint8_t {
        None,
        Rect,
        Image,
    };

    struct alignas(128) text_item final {
        Ui::font_handle_t      font = 0;
        std::string            text;
        Ui::Res::Type::bound_t pos;
        Ui::Color              color;
        Ui::Res::Type::AlignH  alignH  = Ui::Res::Type::AlignH::Left;
        Ui::Res::Type::AlignV  alignV  = Ui::Res::Type::AlignV::Center;
        Ui::fpx_t              minPadH = 0;
    };

    static bool isSvgFile(std::string_view src) { return src.size() >= 4 && src.substr(src.size() - 4) == ".svg"; }

    // Resolve the SvgRenderer document key for an image source (shared by
    // drawImage/warmImage): tinted icons load a colour-filled variant keyed
    // separately from the plain load. Empty when the source is not drawable.
    static std::string resolveImageKey(std::string_view src, bool isTinted)
    {
        if (!isSvgFile(src)) {
            return {}; // only SVG sources are drawn today
        }
        const std::string path = std::string(src);
        std::string       key  = isTinted ? Ui::Gl::SvgRenderer::loadFilledFromFile(path)
                                          : Ui::Gl::SvgRenderer::ensureLoaded(path);
        if (key.empty() || !Ui::Gl::SvgRenderer::isLoaded(key)) {
            return {};
        }
        return key;
    }

    void ensureMode(Mode mode)
    {
        if (m_mode == mode) {
            return;
        }
        endCurrentMode();
        if (mode == Mode::Rect) {
            m_rounded.begin(m_width, m_height, m_scale);
        } else if (mode == Mode::Image) {
            m_svgRenderer.begin(m_width, m_height, m_scale);
        }
        m_mode = mode;
    }

    void endCurrentMode()
    {
        if (m_mode == Mode::Rect) {
            Ui::Gl::Rounded::end();
        } else if (m_mode == Mode::Image) {
            Ui::Gl::SvgRenderer::end();
        }
        m_mode = Mode::None;
    }

    // Layered expanded rounded rects under the shape, decreasing alpha outward.
    void drawShadow(const Ui::Res::Type::bound_t &  bound,
                    const Ui::Res::Type::border_t & radii,
                    const Ui::Render::shadow_t &    shadow)
    {
        int blur = roundToInt(shadow.blur);
        if (blur <= 0) {
            blur = 1;
        }

        const float baseAlpha = (static_cast<float>(shadow.color.a()) / 255.0F) * shadow.opacity;
        const int   layers    = std::min(blur, 5);
        for (int i = layers; i >= 1; --i) {
            const float alpha  = baseAlpha * (1.0F - static_cast<float>(i) / (layers + 1)) * 0.5F;
            const float expand = static_cast<float>(i) * 2;

            const Ui::Res::Type::border_t expanded { radii.topLeft + expand,
                                                     radii.topRight + expand,
                                                     radii.bottomRight + expand,
                                                     radii.bottomLeft + expand };
            const Ui::Color               shadowColor(shadow.color.r(),
                                        shadow.color.g(),
                                        shadow.color.b(),
                                        static_cast<uint8_t>(alpha * 255.0F));
            m_rounded.draw({ bound.x + shadow.offsetX - expand,
                             bound.y + shadow.offsetY - expand,
                             bound.w + expand * 2,
                             bound.h + expand * 2 },
                           expanded,
                           { shadowColor, shadowColor });
        }
    }

    void flushText()
    {
        if (m_textBatch.empty()) {
            return;
        }

        // Batch by font to minimize GL state changes.
        std::sort(m_textBatch.begin(), m_textBatch.end(), [](const text_item & a, const text_item & b) {
            return a.font < b.font;
        });

        Ui::font_handle_t                  prevFont = 0;
        Ui::Gl::FontRenderer::font_rec_t * fr       = nullptr;
        for (auto & item : m_textBatch) {
            if (item.font != prevFont) {
                if (fr != nullptr) {
                    glBindVertexArray(0);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    glUseProgram(0);
                    glDisable(GL_FRAMEBUFFER_SRGB);
                }

                fr       = m_fontRenderer->font(item.font);
                prevFont = item.font;

                if (fr == nullptr || fr->program == 0U) {
                    fr = nullptr;
                    continue;
                }

                glEnable(GL_BLEND);
                glEnable(GL_FRAMEBUFFER_SRGB);
                glUseProgram(fr->program);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, fr->atlas_tex);
                glUniform1i(fr->uTex, 0);
                glUniform1i(fr->uPremultiplied, 0);
                const auto fontProj = Ui::Gl::Util::orthoProjection(m_width, m_height);
                glUniformMatrix4fv(fr->uProjection, 1, GL_FALSE, fontProj.data());
                glBindVertexArray(fr->vao);
                glBindBuffer(GL_ARRAY_BUFFER, fr->vbo);
            }

            if (fr == nullptr) {
                continue;
            }

            // Decode UTF-8 once: advance lookup and glyph emission must agree.
            const std::wstring wtext = Common::Unicode::fromUtf8(item.text);
            const float        textW = Ui::Gl::FontRenderer::measureTextWidth(*fr, wtext);

            const auto baseline = Ui::Gl::TextAlign::baselineY(item.alignV, fr->metrics, item.pos, m_scale);

            if constexpr (Ui::Gl::LOG_TEXT_LAYOUT) {
                static std::set<std::string> loggedTexts;
                if (loggedTexts.insert(item.text).second) {
                    const auto bLine = fr->metrics.baseline(item.pos.y, item.pos.h, m_scale);
                    std::cout << "[Baseline] textop text=\"" << item.text << "\" boxY=" << item.pos.y
                              << " boxH=" << item.pos.h << " baselineCap=" << baseline << " baselineLineBox=" << bLine
                              << " diff=" << (baseline - bLine) << std::endl;
                }
            }

            const Ui::fpx_t textWCss = textW / m_scale;
            const Ui::fpx_t xCursor = Ui::Gl::TextAlign::startX(item.alignH, item.pos, textWCss, item.minPadH, m_scale);

            m_vertexBuffer.clear();
            Ui::Gl::FontRenderer::appendTextVerts(m_vertexBuffer, *fr, wtext, xCursor, baseline);

            if (m_vertexBuffer.empty()) {
                continue;
            }

            auto tf = item.color.toGLRGBA();
            glUniform4f(fr->uColor, tf.at(0), tf.at(1), tf.at(2), tf.at(3));
            Ui::Gl::Util::drawTriangles(m_vertexBuffer);
        }

        if (fr != nullptr) {
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);
            glDisable(GL_FRAMEBUFFER_SRGB);
        }

        m_textBatch.clear();
    }

    void initFlatShader()
    {
        if (m_flatProgram != 0) {
            return;
        }

        const char * vertexSource = R"GLSL(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            uniform mat4 u_projection;
            void main() {
                gl_Position = u_projection * vec4(aPos, 0.0, 1.0);
            }
        )GLSL";

        const char * fragmentSource = R"GLSL(
            #version 330 core
            out vec4 fragColor;
            uniform vec4 u_color;
            void main() {
                fragColor = u_color;
            }
        )GLSL";

        const GLuint vert = Ui::Gl::Util::compileShader(GL_VERTEX_SHADER, vertexSource, "[Flat] Vertex shader error: ");
        const GLuint frag = Ui::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                        fragmentSource,
                                                        "[Flat] Fragment shader error: ");
        if (vert == 0 || frag == 0) {
            if (vert != 0) {
                glDeleteShader(vert);
            }
            if (frag != 0) {
                glDeleteShader(frag);
            }
            return;
        }

        m_flatProgram = Ui::Gl::Util::linkProgram(vert, frag, "[Flat] Shader link error: ");
        if (m_flatProgram == 0) {
            return;
        }

        m_flatUProjection = glGetUniformLocation(m_flatProgram, "u_projection");
        m_flatUColor      = glGetUniformLocation(m_flatProgram, "u_color");

        glGenVertexArrays(1, &m_flatVao);
        glGenBuffers(1, &m_flatVbo);
        glBindVertexArray(m_flatVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_flatVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    Ui::task_fn_t                         m_makeCurrent; // makes the owning window's GL context current (GL teardown)
    Ui::Gl::Rounded                       m_rounded;
    Ui::Gl::SvgRenderer                   m_svgRenderer;
    std::unique_ptr<Ui::Gl::FontRenderer> m_fontRenderer {};

    // Flat-color shader for the loading-bar arrow triangle.
    GLuint m_flatProgram     = 0;
    GLuint m_flatVao         = 0;
    GLuint m_flatVbo         = 0;
    GLint  m_flatUProjection = -1;
    GLint  m_flatUColor      = -1;

    std::vector<text_item> m_textBatch {};
    std::vector<float>     m_vertexBuffer {};

    Mode      m_mode   = Mode::None;
    Ui::fpx_t m_width  = 0;
    Ui::fpx_t m_height = 0;
    float     m_scale  = 1;
};

} // namespace Ui::Gl
