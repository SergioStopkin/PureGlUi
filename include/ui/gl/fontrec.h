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

#include "ui/gl/fonttypes.h"
#include "ui/gl/glyphbitmap.h"
#include "ui/gl/localglew.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/font.h"
#include "ui/type.h"

// NOLINTBEGIN(llvm-include-order)
#include <ft2build.h>
#include FT_FREETYPE_H
// NOLINTEND(llvm-include-order)

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ui::Gl {

// One font at one pixel size FontRenderer caches: its FreeType faces, glyphs, atlas and text shader
// Fallback faces load on demand per missing codepoint via fontconfig
struct alignas(128) font_rec_t final {
    Ui::Res::Type::font_t                       key;      // cache key: family name, scaled size, weight
    std::string                                 filePath; // resolved font file path
    font_metrics_t                              metrics;  // in CSS pixels
    FT_Face                                     ft_face {};
    std::unordered_map<std::string, FT_Face>    fallback_faces; // by font file path, so each loads once at this size
    std::unordered_map<uint32_t, FT_Face>       codepoint_face; // per codepoint; nullptr = no font has it, draw a tofu
    std::unordered_map<wchar_t, glyph_bitmap_t> glyphs;
    Ui::Res::Type::bound_t                      atlas { .x = 0, .y = 0, .w = 1024, .h = 1024 }; // one per font
    fpx_t                                       atlas_row_h = 0;
    std::vector<unsigned char>                  atlas_buf;
    GLuint                                      atlas_tex      = 0;
    GLuint                                      program        = 0; // text shader
    GLuint                                      vao            = 0;
    GLuint                                      vbo            = 0;
    GLint                                       uProjection    = -1;
    GLint                                       uColor         = -1;
    GLint                                       uTex           = -1;
    GLint                                       uPremultiplied = -1;
};

} // namespace Ui::Gl
