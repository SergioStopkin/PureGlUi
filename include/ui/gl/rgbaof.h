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

#include <algorithm>
#include <cairo.h>
#include <cstdint>
#include <glib.h>
#include <iostream>
#include <librsvg/rsvg.h>
#include <optional>
#include <vector>

namespace Ui::Gl {

// An SVG rendered to a `targetW` x `targetH` RGBA pixel buffer, premultiplied or not.
// When content bounds are provided, the viewport is offset so that the ink rect
// is rendered centered within the target surface using uniform scaling (aspect-preserving).
// Returns empty vector on failure.
inline std::vector<uint8_t> rgbaOf(RsvgHandle *                            handle,
                                   int                                     targetW,
                                   int                                     targetH,
                                   const std::optional<content_bounds_t> & crop,
                                   float                                   svgW,
                                   float                                   svgH,
                                   bool                                    isPremultiplied)
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
    RsvgRectangle viewport = { .x      = 0,
                               .y      = 0,
                               .width  = static_cast<double>(targetW),
                               .height = static_cast<double>(targetH) };
    if (crop.has_value() && svgW > 0 && svgH > 0) {
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
            const double vpX = offsetX - (crop->u0 * vpW);
            const double vpY = offsetY - (crop->v0 * vpH);
            viewport         = { .x = vpX, .y = vpY, .width = vpW, .height = vpH };
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
    if (crop.has_value() && svgW > 0 && svgH > 0) {
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
            cairo_translate(cr, offsetX - (crop->u0 * svgW * scale), offsetY - (crop->v0 * svgH * scale));
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
        const uint8_t * srcRow = data + (row * stride); // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        for (int col = 0; col < targetW; ++col) {
            const int dstIdx = ((row * targetW) + col) * 4;
            const int srcOff = col * 4;
            // Cairo ARGB32 on little-endian: bytes are B, G, R, A in memory (premultiplied)
            uint8_t       b = srcRow[srcOff + 0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            uint8_t       g = srcRow[srcOff + 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            uint8_t       r = srcRow[srcOff + 2]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const uint8_t a = srcRow[srcOff + 3]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

            if (!isPremultiplied) {
                if (a > 0 && a < 255) {
                    r = static_cast<uint8_t>(std::min(255, (r * 255) / a));
                    g = static_cast<uint8_t>(std::min(255, (g * 255) / a));
                    b = static_cast<uint8_t>(std::min(255, (b * 255) / a));
                }
            }

            rgba.at(dstIdx + 0) = r;
            rgba.at(dstIdx + 1) = g;
            rgba.at(dstIdx + 2) = b;
            rgba.at(dstIdx + 3) = a;
        }
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    return rgba;
}

} // namespace Ui::Gl
