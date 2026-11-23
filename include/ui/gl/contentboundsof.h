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

#include "ui/gl/contentbounds.h"
#include "ui/type.h"

#include <algorithm>
#include <cairo.h>
#include <cmath>
#include <cstdint>
#include <glib.h>
#include <librsvg/rsvg.h>

namespace Ui::Gl {

// An SVG's tight content bounds, found by rendering it to a reference buffer and scanning
// for non-transparent pixels. This is more reliable than rsvg_handle_get_geometry_for_layer
// which can return wrong bounds for SVGs with empty groups or complex transforms.
inline content_bounds_t contentBoundsOf(RsvgHandle * handle, float intrinsicWidth, float intrinsicHeight)
{
    content_bounds_t cb;
    if (intrinsicWidth <= 0 || intrinsicHeight <= 0) {
        return cb;
    }

    // Render at a reference size (capped for performance)
    constexpr fpx_t kRefMax = 128;
    const fpx_t     aspect  = intrinsicWidth / intrinsicHeight;
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
    const RsvgRectangle viewport = { .x      = 0,
                                     .y      = 0,
                                     .width  = static_cast<double>(refW),
                                     .height = static_cast<double>(refH) };
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
    cairo_scale(cr, static_cast<double>(refW) / intrinsicWidth, static_cast<double>(refH) / intrinsicHeight);
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
        const uint8_t * row = data + (y * stride);
        int             x   = 0;
        while (x < refWi) {
            // Cairo ARGB32 little-endian: byte 3 is alpha
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const uint8_t a = row[(x * 4) + 3];
            if (a > 0) {
                minX = std::min(x, minX);
                maxX = std::max(x, maxX);
                minY = std::min(y, minY);
                maxY = std::max(y, maxY);
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

} // namespace Ui::Gl
