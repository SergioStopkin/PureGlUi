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

#include <algorithm>
#include <vector>

namespace Ui::Animation {

// How long an element's transitions take to run their course: the longest of them; none, no time
[[nodiscard]] inline Common::Time::span_t durationOf(const std::vector<transition_t> & transitions)
{
    if (transitions.empty()) {
        return {};
    }
    return std::ranges::max(transitions, {}, &transition_t::duration).duration;
}

} // namespace Ui::Animation
