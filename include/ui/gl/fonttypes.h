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

#include "ui/type.h"

#include <cmath>
#include <cstdint>

namespace Ui::Gl {

// Native font handle type

// Native font metrics
struct alignas(32) font_metrics_t final {
    int  height      = 0;
    int  ascent      = 0;
    int  descent     = 0;
    int  x_height    = 0;
    bool draw_spaces = true;

    // Line-box baseline Y (physical px) for text vertically centered in a CSS box.
    // Reserves descent space, so mixed-case strings (file names, "Help", "Reload")
    // sit visually centered. Snapped to integer physical pixels because the glyph
    // atlas uses GL_LINEAR - a fractional baseline blurs glyph pixels across rows.
    [[nodiscard]] fpx_t baseline(fpx_t cssY, fpx_t cssH, fpx_t scale) const
    {
        return std::floor((cssY + (cssH - height) / 2.0F + ascent) * scale);
    }

    // Cap-height baseline Y (physical px). Centers the cap block inside the CSS
    // box; lowercase ascenders and descenders are allowed to extend outside the
    // cap block (matches CSS `text-box-edge: cap alphabetic`). Cap height is
    // approximated as the midpoint between x-height and ascender, which lands
    // within ~0.5 phys px of the true cap height for typical sans-serif fonts.
    [[nodiscard]] fpx_t baselineCap(fpx_t cssY, fpx_t cssH, fpx_t scale) const
    {
        const fpx_t capHeight = (ascent + x_height) / 2.0F;
        return std::floor((cssY + (cssH + capHeight) / 2.0F) * scale);
    }

    // Baseline Y for text sitting against the top of the box: the ascender
    // touches the top edge. Padding is the caller's, not baked in here
    [[nodiscard]] fpx_t baselineTop(fpx_t cssY, fpx_t scale) const
    {
        return std::floor((cssY + static_cast<fpx_t>(ascent)) * scale);
    }

    // Baseline Y for text sitting against the bottom: the descender touches
    // the bottom edge
    [[nodiscard]] fpx_t baselineBottom(fpx_t cssY, fpx_t cssH, fpx_t scale) const
    {
        return std::floor((cssY + cssH - static_cast<fpx_t>(descent)) * scale);
    }
};

// Native font style enum
enum class font_style_t : unsigned char { normal, italic, bold };

} // namespace Ui::Gl
