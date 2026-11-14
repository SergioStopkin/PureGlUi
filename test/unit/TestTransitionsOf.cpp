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
 * @file TestTransitionsOf.cpp
 * @brief Unit tests for ui/animation/transitionsof.h, a res `transition` value read as CSS
 *        reads it: a property, a time and an easing in any order, `all` and `ease` when
 *        left out, a part it cannot read moving nothing, and the last of a property kept
 */

#include "common/time.h"
#include "ui/animation/animatedproperty.h"
#include "ui/animation/easing.h"
#include "ui/animation/transition.h"
#include "ui/animation/transitionsof.h"

#include <chrono>
#include <gtest/gtest.h>
#include <vector>

using Ui::Animation::AnimatedProperty;
using Ui::Animation::transition_t;
using Ui::Animation::transitionsOf;

namespace {

constexpr double CLOSE_MS = 1e-3;

// A duration in milliseconds, as the res wrote it near enough: "0.2s" is no whole number of ticks
double msOf(Common::Time::span_t span) { return std::chrono::duration<double, std::milli>(span).count(); }

// The properties the transitions move, in order
std::vector<AnimatedProperty> propertiesOf(const std::vector<transition_t> & transitions)
{
    std::vector<AnimatedProperty> properties;
    for (const transition_t & transition : transitions) {
        properties.emplace_back(transition.property);
    }
    return properties;
}

const std::vector<AnimatedProperty> ALL = {
    AnimatedProperty::Left,   AnimatedProperty::Top,        AnimatedProperty::Width,
    AnimatedProperty::Height, AnimatedProperty::Background, AnimatedProperty::Color,
};

} // namespace

TEST(TransitionsOfTest, OnePropertyTimeAndEasing)
{
    const auto transitions = transitionsOf("left 150ms ease-out");
    ASSERT_EQ(transitions.size(), 1U);
    EXPECT_EQ(transitions.at(0).property, AnimatedProperty::Left);
    EXPECT_NEAR(msOf(transitions.at(0).duration), 150.0, CLOSE_MS);
    EXPECT_EQ(transitions.at(0).easing, Ui::Animation::EASE_OUT);
}

TEST(TransitionsOfTest, SecondsAndEveryKeyword)
{
    EXPECT_NEAR(msOf(transitionsOf("left 0.2s").at(0).duration), 200.0, CLOSE_MS);
    EXPECT_NEAR(msOf(transitionsOf("left .5s").at(0).duration), 500.0, CLOSE_MS);
    EXPECT_EQ(transitionsOf("left 1ms linear").at(0).easing, Ui::Animation::LINEAR);
    EXPECT_EQ(transitionsOf("left 1ms ease").at(0).easing, Ui::Animation::EASE);
    EXPECT_EQ(transitionsOf("left 1ms ease-in").at(0).easing, Ui::Animation::EASE_IN);
    EXPECT_EQ(transitionsOf("left 1ms ease-in-out").at(0).easing, Ui::Animation::EASE_IN_OUT);
}

TEST(TransitionsOfTest, AllAndNoPropertyAreEveryOne)
{
    EXPECT_EQ(propertiesOf(transitionsOf("all 100ms")), ALL);
    EXPECT_EQ(propertiesOf(transitionsOf("100ms")), ALL);
}

TEST(TransitionsOfTest, EaseWhenNoEasing) { EXPECT_EQ(transitionsOf("top 100ms").at(0).easing, Ui::Animation::EASE); }

TEST(TransitionsOfTest, WordsInAnyOrder)
{
    const auto transitions = transitionsOf("ease-in 100ms width");
    ASSERT_EQ(transitions.size(), 1U);
    EXPECT_EQ(transitions.at(0).property, AnimatedProperty::Width);
    EXPECT_EQ(transitions.at(0).easing, Ui::Animation::EASE_IN);
}

TEST(TransitionsOfTest, CubicBezierWithSpacesAndCommas)
{
    const auto transitions = transitionsOf("left 100ms cubic-bezier(0.4, 0, 0.2, 1), top 50ms");
    ASSERT_EQ(transitions.size(), 2U);
    const Ui::Animation::easing_t expected { .x1 = 0.4F, .y1 = 0.0F, .x2 = 0.2F, .y2 = 1.0F };
    EXPECT_EQ(transitions.at(0).easing, expected);
    EXPECT_EQ(transitions.at(1).property, AnimatedProperty::Top);
}

TEST(TransitionsOfTest, CubicBezierMayOvershootInYAlone)
{
    EXPECT_EQ(transitionsOf("left 100ms cubic-bezier(0.34, 1.56, 0.64, 1)").size(), 1U);
    EXPECT_TRUE(transitionsOf("left 100ms cubic-bezier(1.5, 0, 0.2, 1)").empty());
    EXPECT_TRUE(transitionsOf("left 100ms cubic-bezier(0.4, 0, -0.2, 1)").empty());
    EXPECT_TRUE(transitionsOf("left 100ms cubic-bezier(0.4, 0, 0.2)").empty());
    EXPECT_TRUE(transitionsOf("left 100ms cubic-bezier(0.4, 0, 0.2, 1,)").empty());
}

TEST(TransitionsOfTest, SeveralParts)
{
    const auto transitions = transitionsOf("left 100ms, top 50ms linear");
    ASSERT_EQ(transitions.size(), 2U);
    EXPECT_EQ(transitions.at(0).property, AnimatedProperty::Left);
    EXPECT_EQ(transitions.at(1).property, AnimatedProperty::Top);
    EXPECT_NEAR(msOf(transitions.at(1).duration), 50.0, CLOSE_MS);
    EXPECT_EQ(transitions.at(1).easing, Ui::Animation::LINEAR);
}

TEST(TransitionsOfTest, LastOfAPropertyWins)
{
    const auto twice = transitionsOf("left 100ms, left 300ms");
    ASSERT_EQ(twice.size(), 1U);
    EXPECT_NEAR(msOf(twice.at(0).duration), 300.0, CLOSE_MS);

    const auto overAll = transitionsOf("all 100ms, width 300ms");
    ASSERT_EQ(overAll.size(), ALL.size());
    EXPECT_EQ(overAll.back().property, AnimatedProperty::Width);
    EXPECT_NEAR(msOf(overAll.back().duration), 300.0, CLOSE_MS);
}

TEST(TransitionsOfTest, UnreadablePartsMoveNothing)
{
    EXPECT_TRUE(transitionsOf("").empty());
    EXPECT_TRUE(transitionsOf("left").empty());
    EXPECT_TRUE(transitionsOf("left 0ms").empty());
    EXPECT_TRUE(transitionsOf("left -5ms").empty());
    EXPECT_TRUE(transitionsOf("left 5px").empty());
    EXPECT_TRUE(transitionsOf("left bounce 100ms").empty());
    EXPECT_TRUE(transitionsOf("left top 100ms").empty());
    EXPECT_TRUE(transitionsOf("left 100ms ease ease-in").empty());
    EXPECT_TRUE(transitionsOf("left 100ms 50ms").empty()); // a delay, not read
    EXPECT_TRUE(transitionsOf("opacity 100ms").empty());   // not animated
}

TEST(TransitionsOfTest, ColoursByCssName)
{
    EXPECT_EQ(propertiesOf(transitionsOf("background 120ms, color 120ms")),
              (std::vector<AnimatedProperty> { AnimatedProperty::Background, AnimatedProperty::Color }));
}

TEST(TransitionsOfTest, ReadablePartBesideAnUnreadableOne)
{
    const auto transitions = transitionsOf("left 100ms, top, width 50ms");
    EXPECT_EQ(propertiesOf(transitions),
              (std::vector<AnimatedProperty> { AnimatedProperty::Left, AnimatedProperty::Width }));
}
