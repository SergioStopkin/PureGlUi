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

#include "common/cstr.h"
#include "common/noncopyable.h"
#include "common/unicode.h"
#include "ui/config.h"
#include "ui/gl/fonttypes.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/font.h"
#include "ui/type.h"

// NOLINTBEGIN(llvm-include-order)
#include <ft2build.h>
#include FT_FREETYPE_H
// NOLINTEND(llvm-include-order)

#include "ui/gl/glutil.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <fontconfig/fontconfig.h>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Ui::Gl {

// Global flag to enable/disable font renderer debug output
constexpr bool FONT_DEBUG = false;

// Diagnostic output for text vertical-centering work: prints resolved font
// metrics, element bounds, and baseline values. Flip on when debugging.
constexpr bool LOG_TEXT_LAYOUT = false;

/**
 * @brief Font management using FreeType and OpenGL texture atlases
 *
 * This class handles:
 * - FreeType font loading and glyph rasterization
 * - Texture atlas generation for efficient text rendering
 * - Font caching to prevent duplicate atlases
 * - GL shader/VBO management for text rendering
 */
class FontRenderer final : private Common::NonCopyable {
public:
    struct alignas(64) glyph_bitmap_t final {
        fpx_t width   = 0;
        fpx_t height  = 0;
        int   left    = 0;
        int   top     = 0; // bitmap offsets
        int   advance = 0; // in pixels (approx)
        // texture coordinates in atlas
        fpx_t                      u0 = 0;
        fpx_t                      v0 = 0;
        fpx_t                      u1 = 0;
        fpx_t                      v1 = 0;
        std::vector<unsigned char> buf; // grayscale 8bit
    };

    struct alignas(128) font_rec_t final {
        Ui::Res::Type::font_t key;      // Cache key: family name, scaled size, weight
        std::string           filePath; // Resolved font file path
        font_metrics_t        metrics;  // In CSS pixels

        // FreeType handles & cached glyphs. Fallback faces are loaded on demand
        // per missing codepoint via fontconfig. fallback_faces is keyed by font
        // file path so each unique font loads only once at this pixel size.
        // codepoint_face caches per-codepoint resolution (nullptr value = no
        // font on the system has this codepoint - render a tofu glyph instead).
        FT_Face                                     ft_face {};
        std::unordered_map<std::string, FT_Face>    fallback_faces;
        std::unordered_map<uint32_t, FT_Face>       codepoint_face;
        std::unordered_map<wchar_t, glyph_bitmap_t> glyphs;

        // texture atlas (one per font)
        Ui::Res::Type::bound_t     atlas { .x = 0, .y = 0, .w = 1024, .h = 1024 };
        fpx_t                      atlas_row_h = 0;
        std::vector<unsigned char> atlas_buf;
        GLuint                     atlas_tex = 0;

        // GL resources for text rendering
        GLuint program        = 0;
        GLuint vao            = 0;
        GLuint vbo            = 0;
        GLint  uProjection    = -1;
        GLint  uColor         = -1;
        GLint  uTex           = -1;
        GLint  uPremultiplied = -1;
    };

    // makeCurrent makes the owning GL context current; called before GL
    // resource teardown (textures/atlases must be deleted in their context).
    FontRenderer(Ui::task_fn_t makeCurrent, std::string fontDir)
        : m_makeCurrent(std::move(makeCurrent))
        , m_fontDir(std::move(fontDir))
    {
        fcRefCount().fetch_add(1, std::memory_order_acq_rel);
    }

    ~FontRenderer()
    {
        for (auto & p : m_fonts) {
            releaseFontResources(*p.second);
        }
        m_fonts.clear();
        // Last live FontRenderer releases fontconfig's global FcConfig (lazily
        // initialized on the first FcFontMatch call). Without this, LSAN flags
        // the FcConfig allocation as a leak.
        if (fcRefCount().fetch_sub(1, std::memory_order_acq_rel) == 1) {
            FcFini();
        }
    }

    // Live FontRenderer count. The last destructor calls FcFini() to release
    // fontconfig's global FcConfig (otherwise leaked at process exit).
    // Atomic because FontRenderer instances are created and destroyed across
    // render threads (main UI + per-popup/dialog renderers).
    static std::atomic<int> & fcRefCount()
    {
        static std::atomic<int> count { 0 };
        return count;
    }

    // FreeType library handle shared by all font_rec_t instances. Lazy-init on
    // first createFont / ensureGlyph call. Never freed (process-lifetime).
    static FT_Library & ftLibrary()
    {
        static FT_Library s_ft {};
        if (s_ft == nullptr && FT_Init_FreeType(&s_ft) != 0) {
            s_ft = nullptr;
        }
        return s_ft;
    }

    Ui::font_handle_t createFont(const Ui::Res::Type::font_t & font, font_metrics_t * fm = nullptr)
    {
        FT_Library s_ft = ftLibrary();
        if (s_ft == nullptr) {
            if constexpr (FONT_DEBUG) {
                std::cerr << "[Font] FreeType init failed" << std::endl;
            }
            return 0;
        }

        // Scale font size for HiDPI - size is in CSS pixels
        const int cssSize    = font.size > 0 ? font.size : 12;
        int       scaledSize = toPhysFloor(cssSize);
        if (scaledSize < 1) {
            scaledSize = 1;
        }

        // Check font cache to avoid duplicate atlases
        const Ui::Res::Type::font_t cacheKey { font.family, scaledSize, font.weight };
        auto                        cacheIt = m_fontCache.find(cacheKey);
        if (cacheIt != m_fontCache.end()) {
            // Found existing font with same face+size - reuse it!
            const Ui::font_handle_t cachedHandle = cacheIt->second;
            auto                    fontIt       = m_fonts.find(cachedHandle);
            if (fontIt != m_fonts.end() && fontIt->second != nullptr) {
                // Return cached font handle and fill in metrics
                if (fm != nullptr) {
                    *fm = fontIt->second->metrics;
                }
                if constexpr (FONT_DEBUG) {
                    std::cout << "[Font] Reusing cached font: " << font.family << " size=" << scaledSize
                              << " handle=" << cachedHandle << std::endl;
                }
                return cachedHandle;
            }
        }

        auto f = std::make_unique<font_rec_t>();
        f->key = cacheKey;

        // Populate metrics in CSS pixels
        f->metrics.height      = cssSize;
        f->metrics.ascent      = std::lround(cssSize * 0.75F);
        f->metrics.descent     = cssSize - f->metrics.ascent;
        f->metrics.x_height    = std::lround(cssSize * 0.5F);
        f->metrics.draw_spaces = true;

        // Font file scanning: map "family-variant" keys to file paths.
        // E.g. Cantarell-Regular.otf -> "cantarell-regular", Cantarell-Bold.otf -> "cantarell-bold"
        static std::map<std::string, std::string> fontMap;
        static bool                               fontsScanned = false;

        if (!fontsScanned) {
            fontsScanned                = true;
            const std::string & fontDir = m_fontDir;

            if (std::filesystem::exists(fontDir) && std::filesystem::is_directory(fontDir)) {
                for (const auto & entry : std::filesystem::directory_iterator(fontDir)) {
                    if (entry.is_regular_file()) {
                        const std::string filename = entry.path().filename().string();

                        // Strip extension
                        const size_t dotPos = filename.find_last_of('.');
                        std::string  stem   = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;

                        // Normalize: convert to lowercase
                        std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);

                        fontMap[stem] = entry.path().string();
                        if constexpr (FONT_DEBUG) {
                            std::cout << "[Font] Scanned font: " << stem << " -> " << entry.path().string()
                                      << std::endl;
                        }
                    }
                }
            }
        }

        // Build list of font candidates
        std::vector<std::string> candidates {};

        if (!font.family.empty()) {
            // Extract first font family name (before comma, e.g. "Cantarell, sans-serif" -> "Cantarell")
            const size_t commaPos  = font.family.find(',');
            std::string  firstFont = (commaPos != std::string::npos) ? font.family.substr(0, commaPos) : font.family;

            // Trim whitespace and normalize to lowercase
            firstFont.erase(0, firstFont.find_first_not_of(" \t"));
            firstFont.erase(firstFont.find_last_not_of(" \t") + 1);
            std::transform(firstFont.begin(), firstFont.end(), firstFont.begin(), ::tolower);

            // Look up weight-specific variant first, then fall back to other variant
            const std::string boldKey    = firstFont + "-bold";
            const std::string regularKey = firstFont + "-regular";

            if (font.weight == Ui::Res::Type::FontWeight::Bold) {
                if (auto it = fontMap.find(boldKey); it != fontMap.end()) {
                    candidates.emplace_back(it->second);
                } else if (auto it2 = fontMap.find(regularKey); it2 != fontMap.end()) {
                    candidates.emplace_back(it2->second);
                }
            } else {
                if (auto it = fontMap.find(regularKey); it != fontMap.end()) {
                    candidates.emplace_back(it->second);
                } else if (auto it2 = fontMap.find(boldKey); it2 != fontMap.end()) {
                    candidates.emplace_back(it2->second);
                }
            }
        }

        // Fallback to system fonts
        if (font.weight == Ui::Res::Type::FontWeight::Bold) {
            candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf");
            candidates.emplace_back("/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf");
            candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf");
            candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf");
        }
        candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
        candidates.emplace_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
        candidates.emplace_back("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf");
        candidates.emplace_back("/usr/share/fonts/TTF/DejaVuSans.ttf");
        candidates.emplace_back("/System/Library/Fonts/Helvetica.ttc");

        for (const auto & path : candidates) {
            FT_Face faceobj {};
            if (FT_New_Face(s_ft, path.c_str(), 0, &faceobj) == 0) {
                FT_Set_Pixel_Sizes(faceobj, 0, f->key.size);
                f->ft_face  = faceobj;
                f->filePath = path;
                break;
            }
        }

        // If FreeType face loaded, extract more accurate metrics (convert to CSS pixels)
        // and cache glyph bitmaps for ASCII range.
        if (f->ft_face != nullptr) {
            // ascent/descent in scaled pixels, convert to CSS pixels
            const int ascent   = ftToPixels(f->ft_face->size->metrics.ascender);
            const int descent  = ftToPixels(f->ft_face->size->metrics.descender);
            f->metrics.ascent  = toCssFloor(ascent > 0 ? ascent : toPhysFloor(f->metrics.ascent));
            f->metrics.descent = toCssFloor(std::abs(descent));
            f->metrics.height  = f->metrics.ascent + f->metrics.descent;

            // Prepare atlas buffer
            f->atlas_buf.assign(static_cast<size_t>(f->atlas.w * f->atlas.h), 0);
            f->atlas.x     = 0;
            f->atlas.y     = 0;
            f->atlas_row_h = 0;

            // Pre-render ASCII glyphs 32..126 and pack into atlas
            for (wchar_t c = L' '; c <= L'~'; ++c) {
                if (FT_Load_Char(f->ft_face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT | FT_LOAD_FORCE_AUTOHINT) != 0) {
                    continue;
                }
                const FT_Bitmap & bm = f->ft_face->glyph->bitmap;
                glyph_bitmap_t    g;
                g.width   = static_cast<fpx_t>(bm.width);
                g.height  = static_cast<fpx_t>(bm.rows);
                g.left    = f->ft_face->glyph->bitmap_left;
                g.top     = f->ft_face->glyph->bitmap_top;
                g.advance = ftToPixels(f->ft_face->glyph->advance.x);
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                g.buf.assign(bm.buffer, bm.buffer + (bm.width * bm.rows));

                // If glyph too wide for current row, wrap to next row
                if (f->atlas.x + g.width >= f->atlas.w) {
                    f->atlas.x = 0;
                    f->atlas.y += f->atlas_row_h;
                    f->atlas_row_h = 0;
                }

                // If atlas full, stop packing (simple demo - not a full packer)
                if (f->atlas.y + g.height >= f->atlas.h) {
                    break;
                }

                // Copy glyph bitmap into atlas buffer
                int yy = 0;
                while (yy < g.height) {
                    auto dst_off = static_cast<size_t>((f->atlas.y + yy) * f->atlas.w + f->atlas.x);
                    auto src_off = static_cast<size_t>(yy * g.width);
                    std::copy(g.buf.begin() + src_off,
                              g.buf.begin() + src_off + static_cast<size_t>(g.width),
                              f->atlas_buf.begin() + dst_off);
                    ++yy;
                }

                // Set texture coords
                g.u0 = f->atlas.x / f->atlas.w;
                g.v0 = f->atlas.y / f->atlas.h;
                g.u1 = (f->atlas.x + g.width) / f->atlas.w;
                g.v1 = (f->atlas.y + g.height) / f->atlas.h;

                // Advance cursor
                f->atlas.x += g.width + 1; // 1 px padding
                f->atlas_row_h = std::max(f->atlas_row_h, g.height + 1);

                f->glyphs.insert_or_assign(c, std::move(g));
            }

            // Upload atlas to GPU texture (if GL is ready)
            {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glGenTextures(1, &f->atlas_tex);
                glBindTexture(GL_TEXTURE_2D, f->atlas_tex);
                // Linear filtering smooths glyph edges at sub-pixel positions.
                // 1px padding between glyphs in the atlas prevents bleed.
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if defined(GL_LUMINANCE)
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_LUMINANCE,
                             static_cast<GLsizei>(f->atlas.w),
                             static_cast<GLsizei>(f->atlas.h),
                             0,
                             GL_LUMINANCE,
                             GL_UNSIGNED_BYTE,
                             f->atlas_buf.data());
#elif defined(GL_RED)
                glTexImage2D(GL_TEXTURE_2D,
                             0,
                             GL_RED,
                             static_cast<GLsizei>(f->atlas.w),
                             static_cast<GLsizei>(f->atlas.h),
                             0,
                             GL_RED,
                             GL_UNSIGNED_BYTE,
                             f->atlas_buf.data());
#else
#error "No single-channel texture format available (need GL_LUMINANCE or GL_RED)"
#endif
                const GLenum terr = glGetError();
                if (terr != GL_NO_ERROR) {
                    if constexpr (FONT_DEBUG) {
                        std::cerr << "[Font] glTexImage2D error=" << terr << "\n";
                    }
                }

                glBindTexture(GL_TEXTURE_2D, 0);
            }

            // Initialize shader + VBO lazily
            auto init_gl_resources = [&]() {
                // Use current OpenGL context instead of forcing main window context.
                // This ensures resources are created in the appropriate context
                // (main window for main renderer, popup window for popup renderer).
                // The caller (HtmlRenderer constructor) should have already made
                // the correct context current before calling createFont.
                if (f->program != 0U) {
                    return;
                }

                const char * vs_src = R"GLSL(
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

                const char * fs_src = R"GLSL(
                #version 330 core
                in vec2 vUV;
                out vec4 fragColor;
                uniform sampler2D uTex;
                uniform vec4 uColor;
                uniform int uPremultiplied;

                // sRGB -> linear conversion (IEC 61966-2-1 exact)
                float srgbToLinear(float s) {
                    return (s <= 0.04045)
                        ? s / 12.92
                        : pow((s + 0.055) / 1.055, 2.4);
                }

                void main() {
                    float a = texture(uTex, vUV).r;

                    vec3 linearColor = vec3(
                        srgbToLinear(uColor.r),
                        srgbToLinear(uColor.g),
                        srgbToLinear(uColor.b));

                    float outA = uColor.a * a;

                    if (uPremultiplied != 0) {
                        fragColor = vec4(linearColor * outA, outA);
                    } else {
                        fragColor = vec4(linearColor, outA);
                    }
                }
                )GLSL";

                const GLuint vs = Ui::Gl::Util::compileShader(GL_VERTEX_SHADER, vs_src, "[Font] Vertex shader error: ");
                const GLuint fs = Ui::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                              fs_src,
                                                              "[Font] Fragment shader error: ");
                if (vs == 0U || fs == 0U) {
                    if (vs != 0U) {
                        glDeleteShader(vs);
                    }
                    if (fs != 0U) {
                        glDeleteShader(fs);
                    }
                    return;
                }

                f->program = Ui::Gl::Util::linkProgram(vs, fs, "[Font] Shader link error: ");
                if (f->program == 0) {
                    return;
                }

                // Get uniform locations
                f->uProjection    = glGetUniformLocation(f->program, "u_projection");
                f->uColor         = glGetUniformLocation(f->program, "uColor");
                f->uTex           = glGetUniformLocation(f->program, "uTex");
                f->uPremultiplied = glGetUniformLocation(f->program, "uPremultiplied");

                Ui::Gl::Util::createPosUvVao(f->vao, f->vbo);
            };

            init_gl_resources();
        }

        if (fm != nullptr) {
            *fm = f->metrics;
        }

        auto handleKey = m_nextFontHandle++;

        if constexpr (FONT_DEBUG) {
            std::cout << "[Font] createFont: " << f->filePath << " size=" << f->key.size << " handle=" << handleKey
                      << std::endl;
        }

        if constexpr (LOG_TEXT_LAYOUT) {
            std::cout << "[FontMetrics] handle=" << handleKey << " size=" << f->key.size
                      << " ascent=" << f->metrics.ascent << " descent=" << f->metrics.descent
                      << " height=" << f->metrics.height << " x_height=" << f->metrics.x_height
                      << " family=" << f->filePath << std::endl;
        }

        m_fonts[handleKey] = std::move(f);

        // Add to font cache to prevent duplicates
        m_fontCache[cacheKey] = handleKey;
        return handleKey;
    }

    void deleteFont(Ui::font_handle_t hFont)
    {
        auto it = m_fonts.find(hFont);
        if (it != m_fonts.end()) {
            releaseFontResources(*it->second);
            m_fonts.erase(it);
        }
    }

    fpx_t textWidth(Ui::font_handle_t hFont, std::string_view text)
    {
        return textWidth(hFont, Common::Unicode::fromUtf8(text));
    }

    fpx_t textWidth(Ui::font_handle_t hFont, std::wstring_view text)
    {
        if (text.empty()) {
            return 0;
        }
        const int len = static_cast<int>(text.size());
        fpx_t     w   = 0;
        auto      it  = m_fonts.find(hFont);
        if (it != m_fonts.end()) {
            auto * fr = it->second.get();
            for (const wchar_t ch : text) {
                ensureGlyph(*fr, ch);
                auto git = fr->glyphs.find(ch);
                if (git != fr->glyphs.end()) {
                    w += git->second.advance;
                } else {
                    w += fr->key.size / 2.0F;
                }
            }
            w = toCss(w);
        } else {
            w = len * 8.0F;
        }
        return w;
    }

    // Returns font_rec_t for rendering in HtmlRenderer::endFrame
    font_rec_t * font(Ui::font_handle_t hFont)
    {
        auto it = m_fonts.find(hFont);
        if (it != m_fonts.end()) {
            return it->second.get();
        }
        return nullptr;
    }

    // Convert FreeType 26.6 fixed-point to integer pixels
    static int ftToPixels(int64_t ft26dot6) { return static_cast<int>(ft26dot6 / 64); }

    // Find a face containing the given codepoint via fontconfig. Caches both
    // the resolved file -> FT_Face mapping and the per-codepoint result so
    // each unique font is opened at most once and each codepoint queried at
    // most once. Returns nullptr if no font on the system has the codepoint.
    static FT_Face resolveFaceForCodepoint(font_rec_t & fr, uint32_t cp)
    {
        const auto cached = fr.codepoint_face.find(cp);
        if (cached != fr.codepoint_face.end()) {
            return cached->second;
        }

        FT_Face     resolved = nullptr;
        FcCharSet * cs       = FcCharSetCreate();
        FcCharSetAddChar(cs, static_cast<FcChar32>(cp));
        FcPattern * pat = FcPatternCreate();
        FcPatternAddCharSet(pat, FC_CHARSET, cs);
        FcPatternAddBool(pat, FC_SCALABLE, FcTrue);
        FcConfigSubstitute(nullptr, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        FcResult    fcResult = FcResultMatch;
        FcPattern * match    = FcFontMatch(nullptr, pat, &fcResult);
        if (match != nullptr) {
            // FcFontMatch always returns *some* match - validate the result
            // actually contains the requested codepoint via the matched
            // pattern's charset before opening the file.
            FcCharSet * actualCs = nullptr;
            const bool  hasChar  = FcPatternGetCharSet(match, FC_CHARSET, 0, &actualCs) == FcResultMatch
                              && actualCs != nullptr && FcCharSetHasChar(actualCs, static_cast<FcChar32>(cp)) == FcTrue;
            FcChar8 * filePtr = nullptr;
            if (hasChar && FcPatternGetString(match, FC_FILE, 0, &filePtr) == FcResultMatch && filePtr != nullptr) {
                const std::string path     = Common::fromCString(filePtr);
                const auto        existing = fr.fallback_faces.find(path);
                if (existing != fr.fallback_faces.end()) {
                    resolved = existing->second;
                } else {
                    FT_Library s_ft = ftLibrary();
                    if (s_ft != nullptr) {
                        FT_Face face {};
                        if (FT_New_Face(s_ft, path.c_str(), 0, &face) == 0) {
                            FT_Set_Pixel_Sizes(face, 0, fr.key.size);
                            fr.fallback_faces[path] = face;
                            resolved                = face;
                            if constexpr (FONT_DEBUG) {
                                std::cout << "[Font] Fallback face loaded for U+" << std::hex << cp << std::dec << ": "
                                          << path << std::endl;
                            }
                        }
                    }
                }
            }
            FcPatternDestroy(match);
        }
        FcPatternDestroy(pat);
        FcCharSetDestroy(cs);

        fr.codepoint_face.insert_or_assign(cp, resolved);
        return resolved;
    }

    // Pack glyph bitmap into the atlas, set UV coords, store under ch, and
    // re-upload the atlas texture. g.buf must hold g.width * g.height bytes.
    static void commitGlyph(font_rec_t & fr, wchar_t ch, glyph_bitmap_t g)
    {
        if (fr.atlas.x + g.width >= fr.atlas.w) {
            fr.atlas.x = 0;
            fr.atlas.y += fr.atlas_row_h;
            fr.atlas_row_h = 0;
        }
        if (fr.atlas.y + g.height >= fr.atlas.h) {
            return; // Atlas full
        }

        int yy = 0;
        while (yy < g.height) {
            auto dst_off = static_cast<size_t>((fr.atlas.y + yy) * fr.atlas.w + fr.atlas.x);
            auto src_off = static_cast<size_t>(yy * g.width);
            std::copy(g.buf.begin() + src_off,
                      g.buf.begin() + src_off + static_cast<size_t>(g.width),
                      fr.atlas_buf.begin() + dst_off);
            ++yy;
        }

        g.u0 = fr.atlas.x / fr.atlas.w;
        g.v0 = fr.atlas.y / fr.atlas.h;
        g.u1 = (fr.atlas.x + g.width) / fr.atlas.w;
        g.v1 = (fr.atlas.y + g.height) / fr.atlas.h;

        fr.atlas.x += g.width + 1;
        fr.atlas_row_h = std::max(fr.atlas_row_h, g.height + 1);

        fr.glyphs.insert_or_assign(ch, std::move(g));

        if (fr.atlas_tex != 0) {
            glBindTexture(GL_TEXTURE_2D, fr.atlas_tex);
#if defined(GL_LUMINANCE)
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            static_cast<GLsizei>(fr.atlas.w),
                            static_cast<GLsizei>(fr.atlas.h),
                            GL_LUMINANCE,
                            GL_UNSIGNED_BYTE,
                            fr.atlas_buf.data());
#elif defined(GL_RED)
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            0,
                            0,
                            static_cast<GLsizei>(fr.atlas.w),
                            static_cast<GLsizei>(fr.atlas.h),
                            GL_RED,
                            GL_UNSIGNED_BYTE,
                            fr.atlas_buf.data());
#endif
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    // Synthesize a hollow-rectangle "tofu" glyph for a codepoint that no
    // installed font can render. Visible feedback beats silent skipping.
    static void insertTofuGlyph(font_rec_t & fr, wchar_t ch)
    {
        const int w = std::max(2, static_cast<int>(fr.key.size) / 2);
        const int h = std::max(2, (static_cast<int>(fr.key.size) * 7) / 10);

        glyph_bitmap_t g;
        g.width       = static_cast<fpx_t>(w);
        g.height      = static_cast<fpx_t>(h);
        g.left        = 1;
        g.top         = h;
        g.advance     = w + 2;
        const auto ws = static_cast<size_t>(w);
        const auto hs = static_cast<size_t>(h);
        g.buf.assign(ws * hs, 0);
        for (int x = 0; x < w; ++x) {
            const auto xs             = static_cast<size_t>(x);
            g.buf[xs]                 = 0xFF;
            g.buf[(hs - 1) * ws + xs] = 0xFF;
        }
        for (int y = 0; y < h; ++y) {
            const auto ys             = static_cast<size_t>(y);
            g.buf[ys * ws]            = 0xFF;
            g.buf[ys * ws + (ws - 1)] = 0xFF;
        }

        commitGlyph(fr, ch, std::move(g));
    }

    // Load a glyph on demand if not already cached in the atlas. Walks the
    // primary face -> fontconfig-resolved fallback face -> tofu glyph chain.
    static void ensureGlyph(font_rec_t & fr, wchar_t ch)
    {
        if (fr.glyphs.contains(ch) || fr.ft_face == nullptr) {
            return;
        }

        FT_Face face = fr.ft_face;
        if (FT_Get_Char_Index(face, static_cast<FT_ULong>(ch)) == 0) {
            face = resolveFaceForCodepoint(fr, static_cast<uint32_t>(ch));
            if (face == nullptr) {
                insertTofuGlyph(fr, ch);
                return;
            }
        }

        if (FT_Load_Char(face,
                         static_cast<FT_ULong>(ch),
                         FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT | FT_LOAD_FORCE_AUTOHINT)
            != 0) {
            insertTofuGlyph(fr, ch);
            return;
        }

        const FT_Bitmap & bm = face->glyph->bitmap;
        glyph_bitmap_t    g;
        g.width   = static_cast<fpx_t>(bm.width);
        g.height  = static_cast<fpx_t>(bm.rows);
        g.left    = face->glyph->bitmap_left;
        g.top     = face->glyph->bitmap_top;
        g.advance = ftToPixels(face->glyph->advance.x);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        g.buf.assign(bm.buffer, bm.buffer + (bm.width * bm.rows));

        commitGlyph(fr, ch, std::move(g));
    }

    // Append text glyphs (physical pixels) to verts; startX is snapped to an integer
    // physical pixel - GL_LINEAR atlas sampling blurs at fractional positions.
    static void
    appendTextVerts(std::vector<float> & verts, font_rec_t & fr, std::wstring_view text, float startX, float baseline)
    {
        float x_cursor = std::floor(startX);
        for (const wchar_t ch : text) {
            ensureGlyph(fr, ch);
            auto git = fr.glyphs.find(ch);
            if (git == fr.glyphs.end()) {
                x_cursor += static_cast<float>(fr.key.size) / 2.0F;
                continue;
            }
            const glyph_bitmap_t & g = git->second;
            if (g.width <= 0 || g.height <= 0) {
                x_cursor += g.advance;
                continue;
            }
            const float gx = x_cursor + g.left;
            const float gy = baseline - g.top;
            verts.insert(verts.end(), { gx, gy, g.u0, g.v0 });
            verts.insert(verts.end(), { gx + g.width, gy, g.u1, g.v0 });
            verts.insert(verts.end(), { gx + g.width, gy + g.height, g.u1, g.v1 });
            verts.insert(verts.end(), { gx + g.width, gy + g.height, g.u1, g.v1 });
            verts.insert(verts.end(), { gx, gy + g.height, g.u0, g.v1 });
            verts.insert(verts.end(), { gx, gy, g.u0, g.v0 });
            x_cursor += g.advance;
        }
    }

    static void
    appendTextVerts(std::vector<float> & verts, font_rec_t & fr, std::string_view text, float startX, float baseline)
    {
        appendTextVerts(verts, fr, Common::Unicode::fromUtf8(text), startX, baseline);
    }

    static std::vector<float> buildTextVerts(font_rec_t & fr, std::wstring_view text, float startX, float baseline)
    {
        std::vector<float> verts;
        verts.reserve(text.size() * 24);
        appendTextVerts(verts, fr, text, startX, baseline);
        return verts;
    }

    static std::vector<float> buildTextVerts(font_rec_t & fr, std::string_view text, float startX, float baseline)
    {
        return buildTextVerts(fr, Common::Unicode::fromUtf8(text), startX, baseline);
    }

    // Measure text width in physical pixels (advance sum)
    static float measureTextWidth(font_rec_t & fr, std::wstring_view text)
    {
        float width = 0;
        for (const wchar_t ch : text) {
            ensureGlyph(fr, ch);
            auto git = fr.glyphs.find(ch);
            if (git == fr.glyphs.end()) {
                width += static_cast<float>(fr.key.size) / 2.0F;
                continue;
            }
            width += git->second.advance;
        }
        return width;
    }

    static float measureTextWidth(font_rec_t & fr, std::string_view text)
    {
        return measureTextWidth(fr, Common::Unicode::fromUtf8(text));
    }

    // Word-wrap text to fit within maxWidth (physical pixels). Splits on '\n',
    // then on spaces, falling back to character-level breaks.
    static std::vector<std::wstring> wrapText(font_rec_t & fr, std::wstring_view text, float maxWidth)
    {
        std::vector<std::wstring> result;
        std::wstring_view         remaining = text;

        while (!remaining.empty()) {
            auto                    nl        = remaining.find(L'\n');
            const std::wstring_view paragraph = remaining.substr(0, nl);

            if (paragraph.empty()) {
                result.emplace_back();
            } else {
                wrapParagraph(result, fr, paragraph, maxWidth);
            }

            if (nl == std::wstring_view::npos) {
                break;
            }
            remaining.remove_prefix(nl + 1);
        }

        return result;
    }

    static std::vector<std::wstring> wrapText(font_rec_t & fr, std::string_view text, float maxWidth)
    {
        return wrapText(fr, Common::Unicode::fromUtf8(text), maxWidth);
    }

private:
    // Release FreeType faces and GL resources owned by font_rec_t and remove
    // its cache entry. GL resources must be deleted in the context where they
    // were created, so the main context is made current here.
    void releaseFontResources(font_rec_t & fr)
    {
        m_fontCache.erase(fr.key);
        if (m_makeCurrent) {
            m_makeCurrent();
        }
        if (fr.ft_face != nullptr) {
            FT_Done_Face(fr.ft_face);
            fr.ft_face = nullptr;
        }
        for (auto & fb : fr.fallback_faces) {
            if (fb.second != nullptr) {
                FT_Done_Face(fb.second);
            }
        }
        fr.fallback_faces.clear();
        fr.codepoint_face.clear();
        Ui::Gl::Util::deleteTexture(fr.atlas_tex);
        Ui::Gl::Util::deleteVertexArray(fr.vao);
        Ui::Gl::Util::deleteBuffer(fr.vbo);
        Ui::Gl::Util::deleteProgram(fr.program);
    }

    static void
    wrapParagraph(std::vector<std::wstring> & result, font_rec_t & fr, std::wstring_view paragraph, float maxWidth)
    {
        std::wstring_view remaining = paragraph;

        while (!remaining.empty()) {
            if (measureTextWidth(fr, remaining) <= maxWidth) {
                result.emplace_back(remaining);
                break;
            }

            size_t breakPos  = 0;
            float  lineWidth = 0;

            for (size_t i = 0; i < remaining.size(); ++i) {
                const wchar_t ch = remaining[i];
                ensureGlyph(fr, ch);
                auto git = fr.glyphs.find(ch);
                if (git == fr.glyphs.end()) {
                    lineWidth += static_cast<float>(fr.key.size) / 2.0F;
                } else {
                    lineWidth += git->second.advance;
                }

                if (lineWidth > maxWidth) {
                    break;
                }

                if (ch == L' ') {
                    breakPos = i;
                }
            }

            if (breakPos == 0) {
                size_t charBreak = 1;
                lineWidth        = 0;
                for (size_t i = 0; i < remaining.size(); ++i) {
                    const wchar_t ch = remaining[i];
                    ensureGlyph(fr, ch);
                    auto git = fr.glyphs.find(ch);
                    if (git == fr.glyphs.end()) {
                        lineWidth += static_cast<float>(fr.key.size) / 2.0F;
                    } else {
                        lineWidth += git->second.advance;
                    }
                    if (lineWidth > maxWidth) {
                        break;
                    }
                    charBreak = i + 1;
                }
                result.emplace_back(remaining.substr(0, charBreak));
                remaining.remove_prefix(charBreak);
            } else {
                result.emplace_back(remaining.substr(0, breakPos));
                remaining.remove_prefix(breakPos + 1);
            }
        }
    }

    Ui::task_fn_t                                                      m_makeCurrent;
    std::string                                                        m_fontDir;
    std::unordered_map<Ui::font_handle_t, std::unique_ptr<font_rec_t>> m_fonts;
    Ui::font_handle_t                                                  m_nextFontHandle = 1;

    std::unordered_map<Ui::Res::Type::font_t, Ui::font_handle_t> m_fontCache;
};

} // namespace Ui::Gl
