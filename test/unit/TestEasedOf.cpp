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
 * @file TestEasedOf.cpp
 * @brief Unit tests for ui/animation/easedof.h, a CSS timing function solved for its time:
 *        the ends fixed, the keywords at the values browsers give, every curve that keeps
 *        its y within 0..1 never running back, and one past it overshooting
 */

#include "ui/animation/easedof.h"
#include "ui/animation/easing.h"
#include "ui/type.h"

#include <algorithm>
#include <array>
#include <gtest/gtest.h>
#include <initializer_list>

using Ui::Animation::easedOf;
using Ui::Animation::easing_t;

namespace {

constexpr Ui::fpx_t CLOSE = 1e-3F;
constexpr int       STEPS = 100;
constexpr Ui::fpx_t STEP  = 1.0F / STEPS; // the time one step on

constexpr std::array KEYWORDS {
    Ui::Animation::LINEAR,   Ui::Animation::EASE,        Ui::Animation::EASE_IN,
    Ui::Animation::EASE_OUT, Ui::Animation::EASE_IN_OUT,
};

} // namespace

TEST(EasedOfTest, EndsAreFixed)
{
    for (const easing_t & easing : KEYWORDS) {
        EXPECT_FLOAT_EQ(easedOf(easing, 0.0F), 0.0F);
        EXPECT_FLOAT_EQ(easedOf(easing, 1.0F), 1.0F);
        EXPECT_FLOAT_EQ(easedOf(easing, -0.5F), 0.0F);
        EXPECT_FLOAT_EQ(easedOf(easing, 1.5F), 1.0F);
    }
}

TEST(EasedOfTest, LinearIsTheTime)
{
    for (int step = 1; step < STEPS; ++step) {
        EXPECT_NEAR(easedOf(Ui::Animation::LINEAR, step * STEP), step * STEP, CLOSE);
    }
}

TEST(EasedOfTest, KeywordsMatchBrowsers)
{
    EXPECT_NEAR(easedOf(Ui::Animation::EASE, 0.5F), 0.8024F, CLOSE);
    EXPECT_NEAR(easedOf(Ui::Animation::EASE_IN, 0.5F), 0.3154F, CLOSE);
    EXPECT_NEAR(easedOf(Ui::Animation::EASE_OUT, 0.5F), 0.6846F, CLOSE);
    EXPECT_NEAR(easedOf(Ui::Animation::EASE_IN_OUT, 0.5F), 0.5F, CLOSE);
}

TEST(EasedOfTest, EaseInOutIsSymmetric)
{
    for (int step = 1; step < STEPS; ++step) {
        const Ui::fpx_t there = easedOf(Ui::Animation::EASE_IN_OUT, step * STEP);
        const Ui::fpx_t back  = easedOf(Ui::Animation::EASE_IN_OUT, 1.0F - (step * STEP));
        EXPECT_NEAR(there + back, 1.0F, CLOSE);
    }
}

TEST(EasedOfTest, NeverRunsBackWithinTheEnds)
{
    const easing_t steep { .x1 = 0.0F, .y1 = 1.0F, .x2 = 0.0F, .y2 = 1.0F };
    for (const easing_t & easing :
         { Ui::Animation::EASE, Ui::Animation::EASE_IN, Ui::Animation::EASE_OUT, Ui::Animation::EASE_IN_OUT, steep }) {
        Ui::fpx_t last = 0.0F;
        for (int step = 1; step <= STEPS; ++step) {
            const Ui::fpx_t eased = easedOf(easing, step * STEP);
            EXPECT_GE(eased, last - CLOSE);
            last = eased;
        }
    }
}

TEST(EasedOfTest, OvershootsPastOne)
{
    const easing_t back { .x1 = 0.34F, .y1 = 1.56F, .x2 = 0.64F, .y2 = 1.0F };
    Ui::fpx_t      most = 0.0F;
    for (int step = 1; step < STEPS; ++step) {
        most = std::max(most, easedOf(back, step * STEP));
    }
    EXPECT_GT(most, 1.0F);
}
