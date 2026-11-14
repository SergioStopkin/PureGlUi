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
 * @file TestAnimator.cpp
 * @brief Unit tests for ui/animation/animator.h, where an element is drawn while its bound
 *        glides and its colours fade (FLIP): at its place at once the first time, gliding
 *        from where it was drawn when layout moves it, from where it stands when moved again
 *        mid-glide, from where the pointer let it go after a hold, and at once again once a
 *        sweep forgot it; a colour fading in from none rather than through black, landing
 *        exactly on the theme's, and never restarting a move it changes during
 */

#include "common/time.h"
#include "ui/animation/animatedproperty.h"
#include "ui/animation/animator.h"
#include "ui/animation/easing.h"
#include "ui/animation/transition.h"
#include "ui/color.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/colorpair.h"
#include "ui/type.h"

#include <chrono>
#include <gtest/gtest.h>
#include <tuple>
#include <vector>

using Animator = Ui::Animation::Animator<Ui::id_t>;
using Ui::Animation::AnimatedProperty;
using Ui::Animation::transition_t;
using Ui::Res::Type::bound_t;
using Ui::Res::Type::color_pair_t;

namespace {

constexpr Ui::id_t                  ID = 7;
constexpr std::chrono::milliseconds DURATION { 100 };
constexpr std::chrono::milliseconds HALF { 50 };
constexpr Ui::fpx_t                 CLOSE = 1e-2F;

// `left` along a straight line, so half the time is half the way
const std::vector<transition_t> LEFT = {
    transition_t { .property = AnimatedProperty::Left, .duration = DURATION, .easing = Ui::Animation::LINEAR },
};

constexpr bound_t HOME { .x = 0.0F, .y = 0.0F, .w = 100.0F, .h = 30.0F };
constexpr bound_t AWAY { .x = 100.0F, .y = 0.0F, .w = 100.0F, .h = 30.0F };

// `background` and `color` along a straight line
const std::vector<transition_t> COLOURS = {
    transition_t { .property = AnimatedProperty::Background, .duration = DURATION, .easing = Ui::Animation::LINEAR },
    transition_t { .property = AnimatedProperty::Color, .duration = DURATION, .easing = Ui::Animation::LINEAR },
};

const color_pair_t DARK { .fg = Ui::Color(255, 255, 255), .bg = Ui::Color(0, 0, 0) };
const color_pair_t LIGHT { .fg = Ui::Color(0, 0, 0), .bg = Ui::Color(200, 200, 200) };

} // namespace

TEST(AnimatorTest, FirstDrawnAtItsPlace)
{
    Animator   animator;
    const auto now = Common::Time::now();
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, now), AWAY);
    EXPECT_FALSE(animator.isRunning(now));
}

TEST(AnimatorTest, GlidesFromWhereItWasDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    std::ignore      = animator.boundOf(ID, HOME, LEFT, start);
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start), HOME);
    EXPECT_TRUE(animator.isRunning(start));
    EXPECT_NEAR(animator.boundOf(ID, AWAY, LEFT, start + HALF).x, 50.0F, CLOSE);
    EXPECT_TRUE(animator.isRunning(start + HALF));
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start + DURATION), AWAY);
    EXPECT_FALSE(animator.isRunning(start + DURATION));
}

TEST(AnimatorTest, MovedAgainGlidesFromWhereItStands)
{
    Animator   animator;
    const auto start = Common::Time::now();
    std::ignore      = animator.boundOf(ID, HOME, LEFT, start);
    std::ignore      = animator.boundOf(ID, AWAY, LEFT, start);
    EXPECT_NEAR(animator.boundOf(ID, HOME, LEFT, start + HALF).x, 50.0F, CLOSE);
    EXPECT_NEAR(animator.boundOf(ID, HOME, LEFT, start + HALF + HALF).x, 25.0F, CLOSE);
    EXPECT_TRUE(animator.isRunning(start + DURATION));
    EXPECT_FALSE(animator.isRunning(start + HALF + DURATION));
}

TEST(AnimatorTest, NoTransitionsStandsAtItsPlace)
{
    Animator   animator;
    const auto start = Common::Time::now();
    std::ignore      = animator.boundOf(ID, HOME, {}, start);
    EXPECT_EQ(animator.boundOf(ID, AWAY, {}, start), AWAY);
    EXPECT_FALSE(animator.isRunning(start));
}

TEST(AnimatorTest, OnlyTheTransitionedPropertyGlides)
{
    Animator      animator;
    const auto    start = Common::Time::now();
    const bound_t wide { .x = 100.0F, .y = 10.0F, .w = 300.0F, .h = 30.0F };
    std::ignore         = animator.boundOf(ID, HOME, LEFT, start);
    const bound_t moved = animator.boundOf(ID, wide, LEFT, start + HALF);
    EXPECT_FLOAT_EQ(moved.x, HOME.x);
    EXPECT_FLOAT_EQ(moved.y, wide.y);
    EXPECT_FLOAT_EQ(moved.w, wide.w);
}

TEST(AnimatorTest, ReleasedFromAHoldGlidesFromWhereItWasLet)
{
    Animator      animator;
    const auto    start = Common::Time::now();
    const bound_t dragged { .x = 300.0F, .y = 0.0F, .w = 100.0F, .h = 30.0F };
    std::ignore = animator.boundOf(ID, HOME, LEFT, start);
    animator.hold(ID, dragged);
    EXPECT_FALSE(animator.isRunning(start));
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start), dragged);
    EXPECT_NEAR(animator.boundOf(ID, AWAY, LEFT, start + HALF).x, 200.0F, CLOSE);
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start + DURATION), AWAY);
}

TEST(AnimatorTest, SweepForgetsWhatWasNotDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    std::ignore      = animator.boundOf(ID, HOME, LEFT, start);
    animator.sweep();
    animator.sweep();
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start), AWAY);
}

TEST(AnimatorTest, SweepKeepsWhatWasDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    std::ignore      = animator.boundOf(ID, HOME, LEFT, start);
    animator.sweep();
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT, start), HOME);
}

TEST(AnimatorTest, ColoursFadeFromWhereTheyWereDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    EXPECT_EQ(animator.colorsOf(ID, DARK, COLOURS, start), DARK);
    EXPECT_EQ(animator.colorsOf(ID, LIGHT, COLOURS, start), DARK);
    EXPECT_TRUE(animator.isRunning(start));
    const color_pair_t half = animator.colorsOf(ID, LIGHT, COLOURS, start + HALF);
    EXPECT_EQ(half.bg, Ui::Color(100, 100, 100));
    EXPECT_EQ(half.fg, Ui::Color(128, 128, 128));
    EXPECT_EQ(animator.colorsOf(ID, LIGHT, COLOURS, start + DURATION), LIGHT);
    EXPECT_FALSE(animator.isRunning(start + DURATION));
}

TEST(AnimatorTest, FadesInFromNoneAndOutToNone)
{
    Animator           animator;
    const auto         start = Common::Time::now();
    const color_pair_t none { .fg = DARK.fg, .bg = Ui::Color::Inherit() };
    const color_pair_t red { .fg = DARK.fg, .bg = Ui::Color(200, 0, 0) };
    std::ignore = animator.colorsOf(ID, none, COLOURS, start);
    std::ignore = animator.colorsOf(ID, red, COLOURS, start);
    EXPECT_EQ(animator.colorsOf(ID, red, COLOURS, start + HALF).bg, Ui::Color(200, 0, 0, 128));

    const auto later = start + DURATION;
    EXPECT_EQ(animator.colorsOf(ID, red, COLOURS, later).bg, red.bg);
    std::ignore = animator.colorsOf(ID, none, COLOURS, later);
    EXPECT_EQ(animator.colorsOf(ID, none, COLOURS, later + HALF).bg, Ui::Color(200, 0, 0, 128));
    EXPECT_TRUE(animator.colorsOf(ID, none, COLOURS, later + DURATION).bg.isInherit());
}

TEST(AnimatorTest, AColourChangeLeavesAMoveGliding)
{
    Animator                  animator;
    const auto                start = Common::Time::now();
    std::vector<transition_t> both  = LEFT;
    both.insert(both.end(), COLOURS.begin(), COLOURS.end());
    std::ignore = animator.boundOf(ID, HOME, both, start);
    std::ignore = animator.colorsOf(ID, DARK, both, start);
    std::ignore = animator.boundOf(ID, AWAY, both, start);
    std::ignore = animator.colorsOf(ID, LIGHT, both, start + HALF);
    EXPECT_NEAR(animator.boundOf(ID, AWAY, both, start + HALF).x, 50.0F, CLOSE);
    EXPECT_EQ(animator.boundOf(ID, AWAY, both, start + DURATION), AWAY);
}
