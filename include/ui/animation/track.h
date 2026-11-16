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

#include "common/time.h"
#include "ui/animation/transition.h"
#include "ui/type.h"

#include <array>

namespace Ui::Animation {

// One property of one element on its way to where it goes, timed on its own, so a colour
// changing mid-glide never restarts a move. Its value is a number in the first channel or
// a colour's premultiplied RGBA in all four, which is what a colour fades through
struct alignas(128) track_t final {
    std::array<fpx_t, 4>  from {};         // where it stood when its glide began
    std::array<fpx_t, 4>  target {};       // where it goes
    Common::Time::point_t start;           // when its glide began
    transition_t          transition {};   // its glide; none, with no duration: at its target at once
    bool                  isKnown = false; // drawn before: a new target glides from where it stands
};

} // namespace Ui::Animation
