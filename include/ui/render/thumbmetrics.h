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

#include "ui/render/axis.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

namespace Ui::Render {

// What a thumb on a track is derived from, as one parameter rather than four
// repeated at every call. Whoever knows the geometry fills this in; thumb_track_t
// answers where the thumb sits and what a pointer position means.
//
// Deliberately says LENGTH and MAX rather than content and viewport: those are a
// scrollbar's way of arriving at them, and a slider arrives at them differently -
// a fixed thumb over a 0..1 range. Keeping the derivation with the caller is what
// lets both share every line below.
struct alignas(32) thumb_metrics_t final {
    Ui::Res::Type::bound_t track;          // hit area, spanning the axis
    fpx_t                  thumbLength {}; // along the axis
    fpx_t                  maxValue {};    // value with the thumb at the far end
    Axis                   axis = Axis::Vertical;

    [[nodiscard]] fpx_t trackStart() const { return axis == Axis::Vertical ? track.y : track.x; }
    [[nodiscard]] fpx_t trackLength() const { return axis == Axis::Vertical ? track.h : track.w; }

    // How far the thumb's leading edge can move. Shorter than the track by the
    // thumb itself, because the thumb is kept fully inside the ends - which is
    // exactly what a pointer mapping has to agree with, or the thumb never sits
    // where the pointer is.
    [[nodiscard]] fpx_t travel() const
    {
        const fpx_t room = trackLength() - thumbLength;
        return (room > 0.0F) ? room : 0.0F;
    }

    // The thumb, from where its leading edge sits along the axis. Cross-axis it
    // fills the track; a caller drawing something taller overrides that half.
    [[nodiscard]] Ui::Res::Type::bound_t boundAt(fpx_t start) const
    {
        if (axis == Axis::Vertical) {
            return { track.x, start, track.w, thumbLength };
        }
        return { start, track.y, thumbLength, track.h };
    }

    // The pointer coordinate that matters here, so a caller holding both can stay
    // axis-agnostic
    [[nodiscard]] fpx_t along(fpx_t cssX, fpx_t cssY) const { return axis == Axis::Vertical ? cssY : cssX; }

    bool operator==(const thumb_metrics_t &) const = default;
};

} // namespace Ui::Render
