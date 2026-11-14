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
#include "ui/animation/animatedproperty.h"
#include "ui/animation/easing.h"

namespace Ui::Animation {

// One property gliding to its new value whenever it changes, as CSS's `transition`: how
// long, along which curve. An element's res block may carry several (transitionsOf)
struct alignas(32) transition_t final {
    AnimatedProperty     property = AnimatedProperty::Left;
    Common::Time::span_t duration {};
    easing_t             easing {}; // CSS's default, ease

    bool operator==(const transition_t &) const = default;
};

} // namespace Ui::Animation
