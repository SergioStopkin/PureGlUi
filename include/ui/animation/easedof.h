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

#include "ui/animation/easing.h"
#include "ui/type.h"

#include <algorithm>
#include <cmath>

namespace Ui::Animation {

// The way gone at `progress`, the time gone (0..1), along `easing`: the curve's y where its
// x is the time. The curve is parametric, so x is solved for its parameter first - Newton's
// method from the time itself, then halving where the curve runs flat - as browsers do
[[nodiscard]] inline fpx_t easedOf(const easing_t & easing, fpx_t progress)
{
    if (progress <= 0.0F) {
        return 0.0F;
    }
    if (progress >= 1.0F) {
        return 1.0F;
    }
    constexpr int   NEWTON_STEPS  = 8;
    constexpr int   HALVING_STEPS = 32;
    constexpr fpx_t CLOSE_ENOUGH  = 1e-5F;
    const auto      curveAt       = [](fpx_t first, fpx_t second, fpx_t parameter) {
        const fpx_t rest = 1.0F - parameter;
        return (3.0F * rest * rest * parameter * first) + (3.0F * rest * parameter * parameter * second)
             + (parameter * parameter * parameter);
    };
    const auto slopeAt = [](fpx_t first, fpx_t second, fpx_t parameter) {
        const fpx_t rest = 1.0F - parameter;
        return (3.0F * rest * rest * first) + (6.0F * rest * parameter * (second - first))
             + (3.0F * parameter * parameter * (1.0F - second));
    };

    fpx_t parameter = progress;
    for (int step = 0; step < NEWTON_STEPS; ++step) {
        const fpx_t miss = curveAt(easing.x1, easing.x2, parameter) - progress;
        if (std::abs(miss) < CLOSE_ENOUGH) {
            return curveAt(easing.y1, easing.y2, parameter);
        }
        const fpx_t slope = slopeAt(easing.x1, easing.x2, parameter);
        if (std::abs(slope) < CLOSE_ENOUGH) {
            break;
        }
        parameter = std::clamp(parameter - (miss / slope), 0.0F, 1.0F);
    }

    fpx_t low  = 0.0F;
    fpx_t high = 1.0F;
    parameter  = progress;
    for (int step = 0; step < HALVING_STEPS; ++step) {
        const fpx_t time = curveAt(easing.x1, easing.x2, parameter);
        if (std::abs(time - progress) < CLOSE_ENOUGH) {
            break;
        }
        if (time < progress) {
            low = parameter;
        } else {
            high = parameter;
        }
        parameter = (low + high) / 2.0F;
    }
    return curveAt(easing.y1, easing.y2, parameter);
}

} // namespace Ui::Animation
