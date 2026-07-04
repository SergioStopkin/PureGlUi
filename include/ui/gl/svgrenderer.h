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

/**
 * @file SvgRenderer.h
 * @brief Header-only SVG rendering utility using librsvg + Cairo
 *
 * Provides SVG loading, caching, and OpenGL texture rendering
 * for rendering standalone UI icons.
 */

#include "common/bytes.h"
#include "common/noncopyable.h"
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cairo.h>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <librsvg/rsvg.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Thread-local flag: when true, SVG bitmaps will be uploaded to OpenGL
// in their premultiplied ARGB form. This is useful when rendering into an
// ARGB composited popup where we use premultiplied blending.
static thread_local bool s_svg_upload_premultiplied = false;

namespace Ui::Gl {
/**
 * @brief SVG renderer with OpenGL texture caching
 *
 * Features:
 * - Load SVG from file or string
 * - Render to specified size (with HiDPI scale support)
 * - Cache rendered textures by (svg_id, size) pair
 * - Batch rendering support
 */
struct alignas(64) cache_key_t final {
    std::string svgId;
    int         width;
    int         height;

    bool operator==(const cache_key_t & other) const
    {
        return svgId == other.svgId && width == other.width && height == other.height;
    }
};

} // namespace Ui::Gl

template <>
struct std::hash<Ui::Gl::cache_key_t> { // NOLINT(altera-struct-pack-align)
    std::size_t operator()(const Ui::Gl::cache_key_t & k) const
    {
        return std::hash<std::string>()(k.svgId) ^ (std::hash<int>()(k.width) << 1U)
             ^ (std::hash<int>()(k.height) << 2U);
    }
};

namespace Ui::Gl {

class SvgRenderer : private Common::NonCopyable {
public:
    // Control whether textures are uploaded premultiplied for composited popups
    static void setUploadPremultiplied(bool v) { s_svg_upload_premultiplied = v; }

    struct alignas(16) svg_texture_t final {
        GLuint textureId = 0;
        int    width     = 0;
        int    height    = 0;
        bool   valid     = false;
    };

private:
    using RsvgHandlePtr = std::shared_ptr<RsvgHandle>;

    static RsvgHandlePtr wrapHandle(RsvgHandle * raw)
    {
        return { raw, [](RsvgHandle * h) {
                    if (h != nullptr) {
                        g_object_unref(h);
                    }
                } };
    }

    // Cache of rendered SVG textures
    std::unordered_map<cache_key_t, svg_texture_t> textureCache_;

    // Cache of shadow textures (alpha-only mask: RGB=white, A=original shape)
    std::unordered_map<cache_key_t, svg_texture_t> shadowTextureCache_;

    // Cache of SVG intrinsic sizes (width, height) to avoid repeated document queries
    mutable std::unordered_map<std::string, std::pair<float, float>> sizeCache_;

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

        if (s_svg_upload_premultiplied) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }

        auto        sc = shadowColor.toGLRGBA();
        const float sr = sc[0];
        const float sg = sc[1];
        const float sb = sc[2];

        Ui::Gl::Util::forEachShadowLayer(
        shadowOffsetX,
        shadowOffsetY,
        blurRadius,
        shadowOpacity,
        [&](fpx_t ox, fpx_t oy, fpx_t expand, fpx_t alpha) {
            if (s_svg_upload_premultiplied) {
                glUniform4f(m_uColor, sr * alpha, sg * alpha, sb * alpha, alpha);
            } else {
                glUniform4f(m_uColor, sr, sg, sb, alpha);
            }
            drawQuad(bound.x + ox - expand, bound.y + oy - expand, drawW + expand * 2, drawH + expand * 2);
        });

        if (s_svg_upload_premultiplied) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // Tight content bounding box as fractions of intrinsic size (excludes transparent padding)
    struct alignas(16) content_bounds_t final {
        fpx_t u0 = 0.0F; // left fraction (0-1)
        fpx_t v0 = 0.0F; // top fraction (0-1)
        fpx_t u1 = 1.0F; // right fraction (0-1)
        fpx_t v1 = 1.0F; // bottom fraction (0-1)
    };

    // Process-wide shared document and size cache to avoid reparsing the same SVG in multiple renderer instances
    inline static std::unordered_map<std::string, RsvgHandlePtr>           s_documentCacheShared_;
    inline static std::unordered_map<std::string, std::pair<float, float>> s_sizeCacheShared_;
    inline static std::unordered_map<std::string, content_bounds_t>        s_contentBoundsShared_;
    inline static std::mutex                                               s_docMutex_;

    // Per-path load futures to prevent concurrent duplicate parses
    inline static std::unordered_map<std::string, std::shared_future<RsvgHandlePtr>> s_loadFutures_;
    inline static std::mutex                                                         s_futuresMutex_;

    // Instrumentation: counts/timings and per-path counters (thread-safe)
    inline static std::atomic<size_t>  s_loadFromFileCalled { 0 };
    inline static std::atomic<size_t>  s_loadFromFileSucceeded { 0 };
    inline static std::atomic<int64_t> s_loadFromFileNs { 0 };

    inline static std::atomic<size_t>  s_loadFromStringCalled { 0 };
    inline static std::atomic<size_t>  s_loadFromStringSucceeded { 0 };
    inline static std::atomic<int64_t> s_loadFromStringNs { 0 };

    inline static std::atomic<size_t>  s_textureCalled { 0 };
    inline static std::atomic<int64_t> s_textureNs { 0 };

    inline static std::atomic<size_t> s_intrinsicSizeCalled { 0 };

    inline static std::mutex                              s_instrMutex;
    inline static std::unordered_map<std::string, size_t> s_loadsByPath;
    inline static std::unordered_map<std::string, size_t> s_intrinsicByPath;

    // Deduplicate "Loaded SVG" logs across renderer instances/windows
    inline static std::unordered_set<std::string> s_loggedLoadedSvgs_;

    // Helper: query intrinsic pixel size from an RsvgHandle.
    // SVGs without explicit width/height (only viewBox) return sub-pixel intrinsic sizes
    // from rsvg_handle_get_intrinsic_size_in_pixels, so we fall back to the viewBox dimensions.
    static bool queryIntrinsicSize(RsvgHandle * handle, float & outW, float & outH)
    {
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
        // librsvg 2.46+: use intrinsic dimensions API (viewBox + explicit width/height)
        gboolean      hasViewbox = FALSE;
        RsvgRectangle viewbox    = {};
        gboolean      hasW       = FALSE;
        gboolean      hasH       = FALSE;
        RsvgLength    rsvgW      = {};
        RsvgLength    rsvgH      = {};
        rsvg_handle_get_intrinsic_dimensions(handle, &hasW, &rsvgW, &hasH, &rsvgH, &hasViewbox, &viewbox);

        if (hasW != 0 && hasH != 0 && rsvgW.unit == RSVG_UNIT_PX && rsvgH.unit == RSVG_UNIT_PX && rsvgW.length > 1
            && rsvgH.length > 1) {
            outW = static_cast<float>(rsvgW.length);
            outH = static_cast<float>(rsvgH.length);
            return true;
        }

        if (hasViewbox != 0 && viewbox.width > 0 && viewbox.height > 0) {
            outW = static_cast<float>(viewbox.width);
            outH = static_cast<float>(viewbox.height);
            return true;
        }

        gdouble        w       = 0;
        gdouble        h       = 0;
        const gboolean hasSize = rsvg_handle_get_intrinsic_size_in_pixels(handle, &w, &h);
        if (hasSize != 0 && w > 1 && h > 1) {
            outW = static_cast<float>(w);
            outH = static_cast<float>(h);
            return true;
        }
#else
        // librsvg < 2.46: use deprecated dimensions API
        RsvgDimensionData dim = {};
        rsvg_handle_get_dimensions(handle, &dim);
        if (dim.width > 0 && dim.height > 0) {
            outW = static_cast<float>(dim.width);
            outH = static_cast<float>(dim.height);
            return true;
        }
#endif
        return false;
    }

    // Helper: compute tight content bounds by rendering to a reference buffer and scanning
    // for non-transparent pixels. This is more reliable than rsvg_handle_get_geometry_for_layer
    // which can return wrong bounds for SVGs with empty groups or complex transforms.
    static content_bounds_t queryContentBounds(RsvgHandle * handle, float intrW, float intrH)
    {
        content_bounds_t cb;
        if (intrW <= 0 || intrH <= 0) {
            return cb;
        }

        // Render at a reference size (capped for performance)
        constexpr fpx_t kRefMax = 128;
        const fpx_t     aspect  = intrW / intrH;
        fpx_t           refW    = 0;
        fpx_t           refH    = 0;
        if (aspect >= 1.0F) {
            refW = kRefMax;
            refH = std::max(1.0F, std::round(kRefMax / aspect));
        } else {
            refH = kRefMax;
            refW = std::max(1.0F, std::round(kRefMax * aspect));
        }

        cairo_surface_t * surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                                               static_cast<int>(refW),
                                                               static_cast<int>(refH));
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            return cb;
        }

        cairo_t * cr = cairo_create(surface);
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
        const RsvgRectangle viewport = { 0, 0, static_cast<double>(refW), static_cast<double>(refH) };
        GError *            err      = nullptr;
        if (rsvg_handle_render_document(handle, cr, &viewport, &err) == 0) {
            if (err != nullptr) {
                g_error_free(err);
            }
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return cb;
        }
#else
        cairo_scale(cr, static_cast<double>(refW) / intrW, static_cast<double>(refH) / intrH);
        rsvg_handle_render_cairo(handle, cr);
#endif
        cairo_surface_flush(surface);

        const uint8_t * data   = cairo_image_surface_get_data(surface);
        const int       stride = cairo_image_surface_get_stride(surface);

        // Scan for non-transparent pixels (alpha > 0)
        const int refWi = static_cast<int>(refW);
        const int refHi = static_cast<int>(refH);
        int       minX  = refWi;
        int       minY  = refHi;
        int       maxX  = -1;
        int       maxY  = -1;
        int       y     = 0;
        while (y < refHi) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const uint8_t * row = data + y * stride;
            int             x   = 0;
            while (x < refWi) {
                // Cairo ARGB32 little-endian: byte 3 is alpha
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                const uint8_t a = row[x * 4 + 3];
                if (a > 0) {
                    if (x < minX) {
                        minX = x;
                    }
                    if (x > maxX) {
                        maxX = x;
                    }
                    if (y < minY) {
                        minY = y;
                    }
                    if (y > maxY) {
                        maxY = y;
                    }
                }
                ++x;
            }
            ++y;
        }

        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        if (maxX >= minX && maxY >= minY) {
            cb.u0 = minX / refW;
            cb.v0 = minY / refH;
            cb.u1 = (maxX + 1) / refW;
            cb.v1 = (maxY + 1) / refH;
        }
        return cb;
    }

    // Helper: render SVG to RGBA pixel buffer.
    // When content bounds are provided, the viewport is offset so that the ink rect
    // is rendered centered within the target surface using uniform scaling (aspect-preserving).
    // Returns empty vector on failure. Output is RGBA (unpremultiplied or premultiplied depending on flag).
    static std::vector<uint8_t> renderToRGBA(RsvgHandle *             handle,
                                             int                      targetW,
                                             int                      targetH,
                                             const content_bounds_t * crop = nullptr,
                                             float                    svgW = 0,
                                             float                    svgH = 0)
    {
        cairo_surface_t * surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, targetW, targetH);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            return {};
        }

        cairo_t * cr = cairo_create(surface);

#if LIBRSVG_CHECK_VERSION(2, 46, 0)
        // When cropping, compute a viewport that maps the ink rect centered into the target.
        // Uses uniform scaling to preserve aspect ratio (like CSS "object-fit: contain").
        RsvgRectangle viewport = { 0, 0, static_cast<double>(targetW), static_cast<double>(targetH) };
        if (crop != nullptr && svgW > 0 && svgH > 0) {
            const double cbW = crop->u1 - crop->u0;
            const double cbH = crop->v1 - crop->v0;
            if (cbW > 0.0 && cbH > 0.0) {
                // Content size in SVG units
                const double contentSvgW = cbW * svgW;
                const double contentSvgH = cbH * svgH;

                // Uniform scale: fit content into target ("contain")
                const double scale = std::min(targetW / contentSvgW, targetH / contentSvgH);

                // Full viewport at this scale
                const double vpW = svgW * scale;
                const double vpH = svgH * scale;

                // Center the content within the target
                const double renderedW = contentSvgW * scale;
                const double renderedH = contentSvgH * scale;
                const double offsetX   = (targetW - renderedW) / 2.0;
                const double offsetY   = (targetH - renderedH) / 2.0;

                // Offset viewport so content region lands at (offsetX, offsetY)
                const double vpX = offsetX - crop->u0 * vpW;
                const double vpY = offsetY - crop->v0 * vpH;
                viewport         = { vpX, vpY, vpW, vpH };
            }
        }

        GError * err = nullptr;
        if (rsvg_handle_render_document(handle, cr, &viewport, &err) == 0) {
            if (err != nullptr) {
                std::cerr << "[SvgRenderer] Cairo render error: " << err->message << std::endl;
                g_error_free(err);
            }
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return {};
        }
#else
        // librsvg < 2.46: use deprecated render API with cairo transforms
        if (crop != nullptr && svgW > 0 && svgH > 0) {
            const double cbW = crop->u1 - crop->u0;
            const double cbH = crop->v1 - crop->v0;
            if (cbW > 0.0 && cbH > 0.0) {
                const double contentSvgW = cbW * svgW;
                const double contentSvgH = cbH * svgH;
                const double scale       = std::min(targetW / contentSvgW, targetH / contentSvgH);
                const double renderedW   = contentSvgW * scale;
                const double renderedH   = contentSvgH * scale;
                const double offsetX     = (targetW - renderedW) / 2.0;
                const double offsetY     = (targetH - renderedH) / 2.0;
                cairo_translate(cr, offsetX - crop->u0 * svgW * scale, offsetY - crop->v0 * svgH * scale);
                cairo_scale(cr, scale, scale);
            }
        } else if (svgW > 0 && svgH > 0) {
            cairo_scale(cr, static_cast<double>(targetW) / svgW, static_cast<double>(targetH) / svgH);
        }
        rsvg_handle_render_cairo(handle, cr);
#endif

        cairo_surface_flush(surface);

        const uint8_t * data   = cairo_image_surface_get_data(surface);
        const int       stride = cairo_image_surface_get_stride(surface);

        std::vector<uint8_t> rgba(targetW * targetH * 4);

        for (int row = 0; row < targetH; ++row) {
            const uint8_t * srcRow = data + row * stride; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            for (int col = 0; col < targetW; ++col) {
                const int dstIdx = (row * targetW + col) * 4;
                const int srcOff = col * 4;
                // Cairo ARGB32 on little-endian: bytes are B, G, R, A in memory (premultiplied)
                uint8_t       b = srcRow[srcOff + 0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                uint8_t       g = srcRow[srcOff + 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                uint8_t       r = srcRow[srcOff + 2]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                const uint8_t a = srcRow[srcOff + 3]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

                if (!s_svg_upload_premultiplied) {
                    if (a > 0 && a < 255) {
                        r = static_cast<uint8_t>(std::min(255, (r * 255) / a));
                        g = static_cast<uint8_t>(std::min(255, (g * 255) / a));
                        b = static_cast<uint8_t>(std::min(255, (b * 255) / a));
                    }
                }

                rgba[dstIdx + 0] = r;
                rgba[dstIdx + 1] = g;
                rgba[dstIdx + 2] = b;
                rgba[dstIdx + 3] = a;
            }
        }

        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        return rgba;
    }

public:
    SvgRenderer() = default;

    ~SvgRenderer()
    {
        clearCache();
        cleanupGl();
    }

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
     * @brief Load SVG from file and cache the document
     * @param path File path to SVG
     * @return Document ID (the path) or empty string on failure
     */
    static std::string loadFromFile(const std::string & path)
    {
        s_loadFromFileCalled.fetch_add(1);

        // Fast check if already present
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            if (s_documentCacheShared_.find(path) != s_documentCacheShared_.end()) {
                return path;
            }
        }

        // If another thread is loading this path, wait for its future
        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            auto                              it = s_loadFutures_.find(path);
            if (it != s_loadFutures_.end()) {
                auto spdoc = it->second.get();
                if (spdoc != nullptr) {
                    return path;
                }
                // previous load failed; fall through and try again
            }
        }

        // Mark that we are loading this path by creating a promise
        std::promise<RsvgHandlePtr> prom;
        auto                        fut = prom.get_future().share();
        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            s_loadFutures_[path] = fut;
        }

        {
            const std::lock_guard<std::mutex> lock(s_instrMutex);
            s_loadsByPath[path]++;
        }

        auto         t0    = std::chrono::steady_clock::now();
        GError *     gerr  = nullptr;
        GFile *      gfile = g_file_new_for_path(path.c_str());
        RsvgHandle * raw   = rsvg_handle_new_from_gfile_sync(gfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &gerr);
        g_object_unref(gfile);
        auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        s_loadFromFileNs.fetch_add(dur);

        if (raw == nullptr) {
            std::cerr << "[SvgRenderer] Failed to load SVG: " << path;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            // Notify waiters of failure
            prom.set_value(nullptr);
            {
                const std::lock_guard<std::mutex> lock(s_futuresMutex_);
                s_loadFutures_.erase(path);
            }
            return "";
        }

        s_loadFromFileSucceeded.fetch_add(1);

        auto sdoc = wrapHandle(raw);

        // Query intrinsic size
        float w = 0;
        float h = 0;
        queryIntrinsicSize(raw, w, h);

        // Compute tight content bounding box (excludes transparent padding)
        const content_bounds_t cb = queryContentBounds(raw, w, h);

        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            s_documentCacheShared_[path] = sdoc;
            s_sizeCacheShared_[path]     = { w, h };
            s_contentBoundsShared_[path] = cb;
        }

        // Notify waiters with the loaded document
        prom.set_value(sdoc);

        if (s_loggedLoadedSvgs_.insert(path).second) {
            std::cout << "[SvgRenderer] Loaded SVG: " << path << " -> " << w << "x" << h << " content=[" << cb.u0 << ","
                      << cb.v0 << "," << cb.u1 << "," << cb.v1 << "]" << std::endl;
        }

        // Clean up future entry
        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            s_loadFutures_.erase(path);
        }

        return path;
    }

    /**
     * @brief Ensure the SVG is loaded without incrementing the load counter when already cached
     * @param path File path to SVG
     * @return Document ID or empty string on failure
     */
    static std::string ensureLoaded(const std::string & path)
    {
        // Fast path: already loaded in shared cache
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            if (s_documentCacheShared_.find(path) != s_documentCacheShared_.end()) {
                return path;
            }
        }

        return loadFromFile(path);
    }

    /**
     * @brief Load SVG from file with fill="none" replaced by fill="#000000"
     * Used for outline-only SVGs that need to be tinted as solid shapes at runtime.
     * @return Document ID (prefixed with "filled:") or empty string on failure
     */
    static std::string loadFilledFromFile(const std::string & path)
    {
        std::string filledId = "filled:" + path;

        // Check if already loaded
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            if (s_documentCacheShared_.find(filledId) != s_documentCacheShared_.end()) {
                return filledId;
            }
        }

        // Read file content
        std::ifstream file(path);
        if (!file.is_open()) {
            return {};
        }
        std::string svgData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Replace fill="none" with fill="#000000"
        const std::string_view target      = "fill=\"none\"";
        const std::string_view replacement = "fill=\"#000000\"";
        size_t                 pos         = 0;
        while ((pos = svgData.find(target, pos)) != std::string::npos) {
            svgData.replace(pos, target.size(), replacement);
            pos += replacement.size();
        }

        return loadFromString(svgData, filledId);
    }

    /**
     * @brief Load SVG from string data
     * @param svgData SVG XML string
     * @param id Unique identifier for caching
     * @return Document ID or empty string on failure
     */
    static std::string loadFromString(const std::string & svgData, const std::string & id)
    {
        s_loadFromStringCalled.fetch_add(1);

        // Fast check
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            if (s_documentCacheShared_.find(id) != s_documentCacheShared_.end()) {
                return id;
            }
        }

        // Wait for existing load if present
        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            auto                              it = s_loadFutures_.find(id);
            if (it != s_loadFutures_.end()) {
                auto spdoc = it->second.get();
                if (spdoc != nullptr) {
                    return id;
                }
            }
        }

        std::promise<RsvgHandlePtr> prom;
        auto                        fut = prom.get_future().share();
        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            s_loadFutures_[id] = fut;
        }

        auto         t0   = std::chrono::steady_clock::now();
        GError *     gerr = nullptr;
        RsvgHandle * raw  = rsvg_handle_new_from_data(Common::asBytes(svgData.data()),
                                                     static_cast<gsize>(svgData.size()),
                                                     &gerr);
        auto dur = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        s_loadFromStringNs.fetch_add(dur);

        if (raw == nullptr) {
            std::cerr << "[SvgRenderer] Failed to parse SVG string: " << id;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            prom.set_value(nullptr);
            {
                const std::lock_guard<std::mutex> lock(s_futuresMutex_);
                s_loadFutures_.erase(id);
            }
            return "";
        }

        s_loadFromStringSucceeded.fetch_add(1);

        auto sdoc = wrapHandle(raw);

        float w = 0;
        float h = 0;
        queryIntrinsicSize(raw, w, h);

        const content_bounds_t cb = queryContentBounds(raw, w, h);

        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            s_documentCacheShared_[id] = sdoc;
            s_sizeCacheShared_[id]     = { w, h };
            s_contentBoundsShared_[id] = cb;
        }

        prom.set_value(sdoc);

        if (s_loggedLoadedSvgs_.insert(id).second) {
            std::cout << "[SvgRenderer] Loaded SVG from string: " << id << " -> " << w << "x" << h << " content=["
                      << cb.u0 << "," << cb.v0 << "," << cb.u1 << "," << cb.v1 << "]" << std::endl;
        }

        {
            const std::lock_guard<std::mutex> lock(s_futuresMutex_);
            s_loadFutures_.erase(id);
        }

        return id;
    }

    /**
     * @brief Get or render SVG to texture at specified size
     * @param svgId Document ID from load functions
     * @param width Target width in CSS pixels
     * @param height Target height in CSS pixels (0 = preserve aspect ratio)
     * @return Texture info, check .valid for success
     */
    svg_texture_t texture(const std::string & svgId, fpx_t width, fpx_t height = 0)
    {
        RsvgHandlePtr doc;
        float         svgW = 0;
        float         svgH = 0;
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            auto                              it = s_documentCacheShared_.find(svgId);
            if (it != s_documentCacheShared_.end()) {
                doc = it->second;
            }

            if (doc == nullptr) {
                std::cerr << "[SvgRenderer] SVG not found: " << svgId << std::endl;
                return {};
            }

            // Grab cached intrinsic size
            auto sIt = s_sizeCacheShared_.find(svgId);
            if (sIt != s_sizeCacheShared_.end()) {
                svgW = sIt->second.first;
                svgH = sIt->second.second;
            }
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
        const cache_key_t key { svgId, renderWidth, renderHeight };
        auto              cacheIt = textureCache_.find(key);
        if (cacheIt != textureCache_.end()) {
            return cacheIt->second;
        }

        // Look up content bounds for potential cropping
        content_bounds_t cb;
        bool             hasCrop = false;
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            auto                              cbIt = s_contentBoundsShared_.find(svgId);
            if (cbIt != s_contentBoundsShared_.end()) {
                cb = cbIt->second;
                // Consider cropping if content bounds differ meaningfully from full viewBox
                const float cbW = cb.u1 - cb.u0;
                const float cbH = cb.v1 - cb.v0;
                if (cbW > 0.01F && cbH > 0.01F && (cbW < 0.99F || cbH < 0.99F)) {
                    hasCrop = true;
                }
            }
        }

        // Render to bitmap via Cairo.
        // When content bounds are available, offset the viewport so the ink rect
        // is centered within the target using uniform scaling.
        s_textureCalled.fetch_add(1);
        auto t0   = std::chrono::steady_clock::now();
        auto rgba = renderToRGBA(doc.get(), renderWidth, renderHeight, hasCrop ? &cb : nullptr, svgW, svgH);
        auto dur  = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count();
        s_textureNs.fetch_add(dur);

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

        textureCache_[key] = result;

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

        const cache_key_t key { svgId, baseTex.width, baseTex.height };
        auto              cacheIt = shadowTextureCache_.find(key);
        if (cacheIt != shadowTextureCache_.end()) {
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
            const uint8_t a   = rgba[idx + 3];
            if (s_svg_upload_premultiplied) {
                rgba[idx]     = a; // R = A (premultiplied white)
                rgba[idx + 1] = a; // G = A
                rgba[idx + 2] = a; // B = A
            } else {
                rgba[idx]     = 255; // R
                rgba[idx + 1] = 255; // G
                rgba[idx + 2] = 255; // B
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

        shadowTextureCache_[key] = result;
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
        glUniform4f(m_uColor, c[0], c[1], c[2], c[3]);
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
        glUniform4f(m_uColor, c[0], c[1], c[2], c[3]);
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
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW   = bound.w;
        const fpx_t drawH   = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);
        const fpx_t scaledW = drawW * scale;
        const fpx_t scaledH = drawH * scale;
        const fpx_t offsetX = (drawW - scaledW) / 2.0F;
        const fpx_t offsetY = (drawH - scaledH) / 2.0F;

        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform1i(m_uTint, 1);
        auto c = color.toGLRGBA();
        glUniform4f(m_uColor, c[0], c[1], c[2], c[3]);
        drawQuad(bound.x + offsetX, bound.y + offsetY, scaledW, scaledH);
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
        if (m_program == 0) {
            return;
        }
        auto tex = texture(svgId, bound.w, bound.h);
        if (!tex.valid) {
            return;
        }

        const fpx_t drawW   = bound.w;
        const fpx_t drawH   = bound.h > 0 ? bound.h : (bound.w * tex.height / tex.width);
        const fpx_t scaledW = drawW * scale;
        const fpx_t scaledH = drawH * scale;
        const fpx_t offsetX = (drawW - scaledW) / 2.0F;
        const fpx_t offsetY = (drawH - scaledH) / 2.0F;

        glBindTexture(GL_TEXTURE_2D, tex.textureId);
        glUniform4f(m_uColor, 1.0F, 1.0F, 1.0F, 1.0F);
        glUniform1i(m_uTint, 0);
        drawQuad(bound.x + offsetX, bound.y + offsetY, scaledW, scaledH);
    }

    /**
     * @brief Get intrinsic size of SVG document
     * @return (width, height) or (0, 0) if not found
     */
    // Fast path: return intrinsic size only if already cached in process-wide size cache
    // Does NOT increment the instrumentation counters (used to avoid noisy reads for preloaded icons)
    static bool intrinsicSizeIfCached(const std::string & svgId, float & outW, float & outH)
    {
        const std::lock_guard<std::mutex> lock(s_docMutex_);
        auto                              cacheIt = s_sizeCacheShared_.find(svgId);
        if (cacheIt != s_sizeCacheShared_.end()) {
            outW = cacheIt->second.first;
            outH = cacheIt->second.second;
            return true;
        }
        return false;
    }

    // Fast path: return content size (intrinsic size minus padding) only if already cached
    // Returns the tight content dimensions, excluding transparent alpha padding
    static bool contentSizeIfCached(const std::string & svgId, float & outW, float & outH)
    {
        const std::lock_guard<std::mutex> lock(s_docMutex_);

        auto sizeIt = s_sizeCacheShared_.find(svgId);
        if (sizeIt == s_sizeCacheShared_.end()) {
            return false;
        }
        const float intrW = sizeIt->second.first;
        const float intrH = sizeIt->second.second;

        auto cbIt = s_contentBoundsShared_.find(svgId);
        if (cbIt != s_contentBoundsShared_.end()) {
            outW = intrW * (cbIt->second.u1 - cbIt->second.u0);
            outH = intrH * (cbIt->second.v1 - cbIt->second.v0);
        } else {
            outW = intrW;
            outH = intrH;
        }
        return true;
    }

    static std::pair<float, float> intrinsicSize(const std::string & svgId)
    {
        s_intrinsicSizeCalled.fetch_add(1);
        {
            const std::lock_guard<std::mutex> lock(s_instrMutex);
            s_intrinsicByPath[svgId]++;
        }

        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            auto                              cacheIt = s_sizeCacheShared_.find(svgId);
            if (cacheIt != s_sizeCacheShared_.end()) {
                return cacheIt->second;
            }
        }

        // Fallback: query shared document and cache result
        RsvgHandlePtr doc;
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            auto                              it = s_documentCacheShared_.find(svgId);
            if (it != s_documentCacheShared_.end()) {
                doc = it->second;
            } else {
                return { 0.0F, 0.0F };
            }
        }

        float w = 0;
        float h = 0;
        queryIntrinsicSize(doc.get(), w, h);
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            s_sizeCacheShared_[svgId] = { w, h };
        }
        return { w, h };
    }

    /**
     * @brief Check if SVG is loaded
     */
    static bool isLoaded(const std::string & svgId)
    {
        const std::lock_guard<std::mutex> lock(s_docMutex_);
        return s_documentCacheShared_.find(svgId) != s_documentCacheShared_.end();
    }

    /**
     * @brief Remove specific texture from cache (frees GPU memory)
     */
    void removeTexture(const std::string & svgId, int width, int height)
    {
        const cache_key_t key { svgId, width, height };
        auto              it = textureCache_.find(key);
        if (it != textureCache_.end()) {
            if (it->second.textureId != 0U) {
                glDeleteTextures(1, &it->second.textureId);
            }
            textureCache_.erase(it);
        }
    }

    /**
     * @brief Remove all textures for a specific SVG (all sizes)
     */
    void removeAllTextures(const std::string & svgId)
    {
        for (auto it = textureCache_.begin(); it != textureCache_.end();) {
            if (it->first.svgId == svgId) {
                if (it->second.textureId != 0U) {
                    glDeleteTextures(1, &it->second.textureId);
                }
                it = textureCache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Unload SVG document and all its textures
     */
    void unload(const std::string & svgId)
    {
        removeAllTextures(svgId);
        const std::lock_guard<std::mutex> lock(s_docMutex_);
        s_documentCacheShared_.erase(svgId);
        s_sizeCacheShared_.erase(svgId);
        s_contentBoundsShared_.erase(svgId);
    }

    /**
     * @brief Clear all caches (documents and textures)
     */
    void clearCache()
    {
        for (auto & pair : textureCache_) {
            Ui::Gl::Util::deleteTexture(pair.second.textureId);
        }
        textureCache_.clear();

        for (auto & pair : shadowTextureCache_) {
            Ui::Gl::Util::deleteTexture(pair.second.textureId);
        }
        shadowTextureCache_.clear();

        sizeCache_.clear();
    }

    // Clear process-wide shared document/size cache (call explicitly when needed)
    static void clearGlobalCache()
    {
        const std::lock_guard<std::mutex> lock(s_docMutex_);
        s_documentCacheShared_.clear();
        s_sizeCacheShared_.clear();
        s_contentBoundsShared_.clear();
    }

    /**
     * @brief Get cache statistics
     */
    void cacheStats(size_t & documentCount, size_t & textureCount, size_t & estimatedGpuMemory) const
    {
        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            documentCount = s_documentCacheShared_.size();
        }
        textureCount       = textureCache_.size();
        estimatedGpuMemory = 0;
        for (const auto & pair : textureCache_) {
            estimatedGpuMemory += pair.second.width * pair.second.height * 4; // RGBA
        }
    }

    /**
     * @brief Print cache statistics to stdout
     */
    void printCacheStats() const
    {
        size_t docs = 0;
        size_t texs = 0;
        size_t mem  = 0;
        cacheStats(docs, texs, mem);
        std::cout << "[SvgRenderer] Cache: " << docs << " documents, " << texs << " textures, ~" << (mem / 1024)
                  << " KB GPU memory" << std::endl;
    }

    // Test helpers: access/reset instrumentation counters for unit tests
    static size_t intrinsicSizeCalled() { return s_intrinsicSizeCalled.load(); }

    static void resetInstrumentationCounters()
    {
        s_loadFromFileCalled.store(0);
        s_loadFromFileSucceeded.store(0);
        s_loadFromFileNs.store(0);

        s_loadFromStringCalled.store(0);
        s_loadFromStringSucceeded.store(0);
        s_loadFromStringNs.store(0);

        s_textureCalled.store(0);
        s_textureNs.store(0);

        s_intrinsicSizeCalled.store(0);

        {
            const std::lock_guard<std::mutex> lock(s_instrMutex);
            s_loadsByPath.clear();
            s_intrinsicByPath.clear();
        }

        {
            const std::lock_guard<std::mutex> lock(s_docMutex_);
            s_loggedLoadedSvgs_.clear();
        }
    }
};

} // namespace Ui::Gl
