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

/**
 * @file TestDurationOf.cpp
 * @brief Unit tests for ui/animation/durationof.h, how long an element's transitions run:
 *        the longest of them, and none for no transition - which is how long each step of
 *        a dialog's key press holds, so with no animation.json a key acts at once
 */

#include "common/time.h"
#include "ui/animation/animatedproperty.h"
#include "ui/animation/durationof.h"
#include "ui/animation/transition.h"

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

using Ui::Animation::AnimatedProperty;
using Ui::Animation::durationOf;
using Ui::Animation::transition_t;

TEST(DurationOfTest, NoneTakesNoTime) { EXPECT_EQ(durationOf({}), Common::Time::span_t::zero()); }

TEST(DurationOfTest, TheLongestRuns)
{
    const std::vector<transition_t> transitions = {
        transition_t { .property = AnimatedProperty::Background, .duration = std::chrono::milliseconds(120) },
        transition_t { .property = AnimatedProperty::Color, .duration = std::chrono::milliseconds(300) },
        transition_t { .property = AnimatedProperty::Left, .duration = std::chrono::milliseconds(80) },
    };
    EXPECT_EQ(durationOf(transitions), std::chrono::milliseconds(300));
}
