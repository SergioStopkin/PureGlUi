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
#include "ui/render/thumbmetrics.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

namespace Ui::Render {

// The thumb geometry of a dock slider: its track, and how long the thumb is.
//
// Both halves of the widget go through here - the one that DRAWS the thumb and
// the one that turns a pointer into a value - so they cannot disagree about how
// far it travels. They used to: the drawing kept the thumb inside the track ends
// while the pointer mapping spanned the whole width, so the thumb trailed the
// cursor by its own length and jumped when grabbed.
//
// Unlike a scrollbar's, the thumb is a FIXED length and the value is a plain
// 0..1 ratio - there is no content behind it to be proportional to.
//
// The length is passed rather than derived from the track, because res states it
// per pointer state. Callers pass the HOVER length in both states, the rule the
// scrollbar already follows for its own widening: the travel and the hit area
// must be the larger of the two, or the pointer falls out of the grip at the
// moment the grip grows to meet it.
[[nodiscard]] inline thumb_metrics_t sliderMetrics(const Ui::Res::Type::bound_t & track, fpx_t thumbLength)
{
    return { track, thumbLength, 1.0F, Axis::Horizontal };
}

} // namespace Ui::Render
