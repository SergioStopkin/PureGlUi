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

#include <glib.h>
#include <librsvg/rsvg.h>
#include <utility>

namespace Ui::Gl {

// An SVG's intrinsic pixel size, {0, 0} when it has none.
// SVGs without explicit width/height (only viewBox) return sub-pixel intrinsic sizes
// from rsvg_handle_get_intrinsic_size_in_pixels, so we fall back to the viewBox dimensions.
inline std::pair<float, float> intrinsicSizeOf(RsvgHandle * handle)
{
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
    // librsvg 2.46+: use intrinsic dimensions API (viewBox + explicit width/height)
    gboolean      hasViewbox = {};
    RsvgRectangle viewbox    = {};
    gboolean      hasW       = {};
    gboolean      hasH       = {};
    RsvgLength    rsvgW      = {};
    RsvgLength    rsvgH      = {};
    rsvg_handle_get_intrinsic_dimensions(handle, &hasW, &rsvgW, &hasH, &rsvgH, &hasViewbox, &viewbox);

    if (hasW != 0 && hasH != 0 && rsvgW.unit == RSVG_UNIT_PX && rsvgH.unit == RSVG_UNIT_PX && rsvgW.length > 1
        && rsvgH.length > 1) {
        return { static_cast<float>(rsvgW.length), static_cast<float>(rsvgH.length) };
    }

    if (hasViewbox != 0 && viewbox.width > 0 && viewbox.height > 0) {
        return { static_cast<float>(viewbox.width), static_cast<float>(viewbox.height) };
    }

    gdouble        w       = 0;
    gdouble        h       = 0;
    const gboolean hasSize = rsvg_handle_get_intrinsic_size_in_pixels(handle, &w, &h);
    if (hasSize != 0 && w > 1 && h > 1) {
        return { static_cast<float>(w), static_cast<float>(h) };
    }
#else
    // librsvg < 2.46: use deprecated dimensions API
    RsvgDimensionData dim = {};
    rsvg_handle_get_dimensions(handle, &dim);
    if (dim.width > 0 && dim.height > 0) {
        return { static_cast<float>(dim.width), static_cast<float>(dim.height) };
    }
#endif
    return { 0.0F, 0.0F };
}

} // namespace Ui::Gl
