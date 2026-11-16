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
    Animator animator;
    animator.beginFrame(Common::Time::now());
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), AWAY);
    EXPECT_FALSE(animator.isRunning());
}

TEST(AnimatorTest, GlidesFromWhereItWasDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    animator.beginFrame(start);
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), HOME);
    EXPECT_TRUE(animator.isRunning());
    animator.beginFrame(start + HALF);
    EXPECT_NEAR(animator.boundOf(ID, AWAY, LEFT).x, 50.0F, CLOSE);
    EXPECT_TRUE(animator.isRunning());
    animator.beginFrame(start + DURATION);
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), AWAY);
    EXPECT_FALSE(animator.isRunning());
}

TEST(AnimatorTest, MovedAgainGlidesFromWhereItStands)
{
    Animator   animator;
    const auto start = Common::Time::now();
    animator.beginFrame(start);
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    std::ignore = animator.boundOf(ID, AWAY, LEFT);
    animator.beginFrame(start + HALF);
    EXPECT_NEAR(animator.boundOf(ID, HOME, LEFT).x, 50.0F, CLOSE);
    animator.beginFrame(start + DURATION);
    EXPECT_NEAR(animator.boundOf(ID, HOME, LEFT).x, 25.0F, CLOSE);
    EXPECT_TRUE(animator.isRunning());
    animator.beginFrame(start + HALF + DURATION);
    EXPECT_FALSE(animator.isRunning());
}

TEST(AnimatorTest, NoTransitionsStandsAtItsPlace)
{
    Animator animator;
    animator.beginFrame(Common::Time::now());
    std::ignore = animator.boundOf(ID, HOME, {});
    EXPECT_EQ(animator.boundOf(ID, AWAY, {}), AWAY);
    EXPECT_FALSE(animator.isRunning());
}

TEST(AnimatorTest, OnlyTheTransitionedPropertyGlides)
{
    Animator      animator;
    const auto    start = Common::Time::now();
    const bound_t wide { .x = 100.0F, .y = 10.0F, .w = 300.0F, .h = 30.0F };
    animator.beginFrame(start);
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    animator.beginFrame(start + HALF);
    const bound_t moved = animator.boundOf(ID, wide, LEFT);
    EXPECT_FLOAT_EQ(moved.x, HOME.x);
    EXPECT_FLOAT_EQ(moved.y, wide.y);
    EXPECT_FLOAT_EQ(moved.w, wide.w);
}

TEST(AnimatorTest, ReleasedFromAHoldGlidesFromWhereItWasLet)
{
    Animator      animator;
    const auto    start = Common::Time::now();
    const bound_t dragged { .x = 300.0F, .y = 0.0F, .w = 100.0F, .h = 30.0F };
    animator.beginFrame(start);
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    animator.hold(ID, dragged);
    EXPECT_FALSE(animator.isRunning());
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), dragged);
    animator.beginFrame(start + HALF);
    EXPECT_NEAR(animator.boundOf(ID, AWAY, LEFT).x, 200.0F, CLOSE);
    animator.beginFrame(start + DURATION);
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), AWAY);
}

TEST(AnimatorTest, SweepForgetsWhatWasNotDrawn)
{
    Animator animator;
    animator.beginFrame(Common::Time::now());
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    animator.sweep();
    animator.sweep();
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), AWAY);
}

TEST(AnimatorTest, SweepKeepsWhatWasDrawn)
{
    Animator animator;
    animator.beginFrame(Common::Time::now());
    std::ignore = animator.boundOf(ID, HOME, LEFT);
    animator.sweep();
    EXPECT_EQ(animator.boundOf(ID, AWAY, LEFT), HOME);
}

// Colours set rather than faded to stand there at once, with nothing gliding, and the next fade
// starts from them, not from where they were drawn before
TEST(AnimatorTest, SetColoursStandAtOnceTheNextFadeFromThem)
{
    Animator   animator;
    const auto start = Common::Time::now();
    animator.beginFrame(start);
    std::ignore = animator.colorsOf(ID, DARK, COLOURS);
    animator.setColors(ID, LIGHT);
    EXPECT_EQ(animator.colorsOf(ID, LIGHT, COLOURS), LIGHT);
    EXPECT_FALSE(animator.isRunning());
    std::ignore = animator.colorsOf(ID, DARK, COLOURS);
    animator.beginFrame(start + HALF);
    const color_pair_t half = animator.colorsOf(ID, DARK, COLOURS);
    EXPECT_EQ(half.bg, Ui::Color(100, 100, 100));
    EXPECT_EQ(half.fg, Ui::Color(128, 128, 128));
}

// None set - a background inherit - stays none, exactly, as a fade landing there does
TEST(AnimatorTest, SetNoneStaysNone)
{
    Animator           animator;
    const color_pair_t none { .fg = DARK.fg, .bg = Ui::Color::Inherit() };
    animator.beginFrame(Common::Time::now());
    std::ignore = animator.colorsOf(ID, LIGHT, COLOURS);
    animator.setColors(ID, none);
    EXPECT_TRUE(animator.colorsOf(ID, none, COLOURS).bg.isInherit());
}

TEST(AnimatorTest, ColoursFadeFromWhereTheyWereDrawn)
{
    Animator   animator;
    const auto start = Common::Time::now();
    animator.beginFrame(start);
    EXPECT_EQ(animator.colorsOf(ID, DARK, COLOURS), DARK);
    EXPECT_EQ(animator.colorsOf(ID, LIGHT, COLOURS), DARK);
    EXPECT_TRUE(animator.isRunning());
    animator.beginFrame(start + HALF);
    const color_pair_t half = animator.colorsOf(ID, LIGHT, COLOURS);
    EXPECT_EQ(half.bg, Ui::Color(100, 100, 100));
    EXPECT_EQ(half.fg, Ui::Color(128, 128, 128));
    animator.beginFrame(start + DURATION);
    EXPECT_EQ(animator.colorsOf(ID, LIGHT, COLOURS), LIGHT);
    EXPECT_FALSE(animator.isRunning());
}

TEST(AnimatorTest, FadesInFromNoneAndOutToNone)
{
    Animator           animator;
    const auto         start = Common::Time::now();
    const color_pair_t none { .fg = DARK.fg, .bg = Ui::Color::Inherit() };
    const color_pair_t red { .fg = DARK.fg, .bg = Ui::Color(200, 0, 0) };
    animator.beginFrame(start);
    std::ignore = animator.colorsOf(ID, none, COLOURS);
    std::ignore = animator.colorsOf(ID, red, COLOURS);
    animator.beginFrame(start + HALF);
    EXPECT_EQ(animator.colorsOf(ID, red, COLOURS).bg, Ui::Color(200, 0, 0, 128));

    const auto later = start + DURATION;
    animator.beginFrame(later);
    EXPECT_EQ(animator.colorsOf(ID, red, COLOURS).bg, red.bg);
    std::ignore = animator.colorsOf(ID, none, COLOURS);
    animator.beginFrame(later + HALF);
    EXPECT_EQ(animator.colorsOf(ID, none, COLOURS).bg, Ui::Color(200, 0, 0, 128));
    animator.beginFrame(later + DURATION);
    EXPECT_TRUE(animator.colorsOf(ID, none, COLOURS).bg.isInherit());
}

TEST(AnimatorTest, AColourChangeLeavesAMoveGliding)
{
    Animator                  animator;
    const auto                start = Common::Time::now();
    std::vector<transition_t> both  = LEFT;
    both.insert(both.end(), COLOURS.begin(), COLOURS.end());
    animator.beginFrame(start);
    std::ignore = animator.boundOf(ID, HOME, both);
    std::ignore = animator.colorsOf(ID, DARK, both);
    std::ignore = animator.boundOf(ID, AWAY, both);
    animator.beginFrame(start + HALF);
    std::ignore = animator.colorsOf(ID, LIGHT, both);
    EXPECT_NEAR(animator.boundOf(ID, AWAY, both).x, 50.0F, CLOSE);
    animator.beginFrame(start + DURATION);
    EXPECT_EQ(animator.boundOf(ID, AWAY, both), AWAY);
}
