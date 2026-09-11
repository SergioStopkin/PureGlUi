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

namespace Ui::Render {

// The state a one-axis drag has to remember between press and release: where
// the pointer grabbed, what the value was then, and how far the value moves per
// pixel. Every drag in the framework is this - a dock grip resizing a column, a
// scrollbar thumb sliding an offset - differing only in scale and in what the
// caller does with the result.
//
// Deliberately unclamped. The bounds are the caller's: a dock's ceiling depends
// on live viewport space, a thumb's on content height, and neither is knowable
// here.
struct alignas(16) drag_track_t final {
    fpx_t grabPos     = 0.0F; // pointer coordinate along the axis at press
    fpx_t valueAtGrab = 0.0F; // tracked value at that moment
    fpx_t scale       = 1.0F; // value units per pixel; negative mirrors the axis

    void begin(fpx_t pos, fpx_t value, fpx_t unitsPerPixel)
    {
        grabPos     = pos;
        valueAtGrab = value;
        scale       = unitsPerPixel;
    }

    [[nodiscard]] fpx_t valueAt(fpx_t pos) const { return valueAtGrab + ((pos - grabPos) * scale); }

    // Unsigned pointer travel, for the "was this a click or a drag" gate
    [[nodiscard]] fpx_t travelFrom(fpx_t pos) const
    {
        const fpx_t delta = pos - grabPos;
        return delta < 0.0F ? -delta : delta;
    }
};

} // namespace Ui::Render
