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

#include "ui/render/dragtrack.h"
#include "ui/render/thumbmetrics.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <algorithm>

namespace Ui::Render {

// A thumb on a track: geometry, hover and drag, shared by the dialog scrollbar,
// the dock scrollbar and the dock slider.
//
// One place answers both "where is the thumb" and "what does this pointer
// position mean", which is the point: when those two live apart they disagree
// about the travel, and the thumb stops sitting under the pointer.
//
// The VALUE stays with the caller - that is what lets the dialog keep an animated
// pixel offset, a dock a row index, and a slider a 0..1 ratio. So does drawing:
// the dialog needs Rounded's split-corner path, a dock goes through appendBg.
struct alignas(32) thumb_track_t final {
    drag_track_t grab; // grab state while isDragging
    bool         isHoveredTrack = false;
    bool         isHoveredThumb = false;
    bool         isDragging     = false;

    [[nodiscard]] static Ui::Res::Type::bound_t thumbBound(const thumb_metrics_t & metrics, fpx_t value)
    {
        const fpx_t limit = metrics.maxValue;
        const fpx_t ratio = (limit > 0.0F) ? std::clamp(value / limit, 0.0F, 1.0F) : 0.0F;
        return metrics.boundAt(metrics.trackStart() + (metrics.travel() * ratio));
    }

    // The value that puts the thumb's MIDDLE at `pos` - what a press on bare
    // track means for a widget that jumps to the pointer rather than paging.
    [[nodiscard]] static fpx_t centredValue(const thumb_metrics_t & metrics, fpx_t pos)
    {
        const fpx_t travel = metrics.travel();
        if (travel <= 0.0F) {
            return 0.0F;
        }
        const fpx_t start = pos - (metrics.thumbLength / 2.0F) - metrics.trackStart();
        return std::clamp((start / travel) * metrics.maxValue, 0.0F, metrics.maxValue);
    }

    [[nodiscard]] static bool isOnThumb(const thumb_metrics_t & metrics, fpx_t value, fpx_t pos)
    {
        const Ui::Res::Type::bound_t span   = thumbBound(metrics, value);
        const fpx_t                  start  = metrics.axis == Axis::Vertical ? span.y : span.x;
        const fpx_t                  length = metrics.axis == Axis::Vertical ? span.h : span.w;
        return pos >= start && pos < start + length;
    }

    // Thumb travel is shorter than value travel, so a pixel of pointer movement
    // is worth more than a unit of value
    void begin(const thumb_metrics_t & metrics, fpx_t pos, fpx_t value)
    {
        const fpx_t travel   = metrics.travel();
        const fpx_t perPixel = (travel > 0.0F) ? (metrics.maxValue / travel) : 0.0F;
        grab.begin(pos, value, perPixel);
        isDragging = true;
    }

    [[nodiscard]] fpx_t valueAt(const thumb_metrics_t & metrics, fpx_t pos) const
    {
        return std::clamp(grab.valueAt(pos), 0.0F, metrics.maxValue);
    }

    // Whether either flag moved, so a caller can skip a repaint
    bool updateHover(const thumb_metrics_t & metrics, fpx_t value, fpx_t cssX, fpx_t cssY)
    {
        const bool wasTrackHovered = isHoveredTrack;
        const bool wasThumbHovered = isHoveredThumb;
        isHoveredTrack             = metrics.track.contains(cssX, cssY);
        isHoveredThumb             = thumbBound(metrics, value).contains(cssX, cssY);
        return isHoveredTrack != wasTrackHovered || isHoveredThumb != wasThumbHovered;
    }

    void clear()
    {
        isHoveredTrack = false;
        isHoveredThumb = false;
        isDragging     = false;
    }
};

} // namespace Ui::Render
