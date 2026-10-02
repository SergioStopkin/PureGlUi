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
 * @file TestTooltipWantedOf.cpp
 * @brief Unit tests for which tooltip the pointer asks for and when:
 *        ui/window/spentbuttonof.h (a press silences a button's tooltip until the
 *        pointer moves off it), ui/window/tooltipwantedof.h (a button's before the
 *        content surface's) and ui/window/istooltipdelayed.h (the first toolbar
 *        tooltip waits, the next follows at once, a content surface's never waits).
 */

#include "ui/type.h"
#include "ui/window/istooltipdelayed.h"
#include "ui/window/spentbuttonof.h"
#include "ui/window/tooltip.h"
#include "ui/window/tooltipwantedof.h"

#include <gtest/gtest.h>

using Ui::INVALID_ID;
using Ui::Window::isTooltipDelayed;
using Ui::Window::spentButtonOf;
using Ui::Window::tooltip_t;
using Ui::Window::tooltipWantedOf;

namespace {

constexpr Ui::id_t BUTTON = 5;
constexpr Ui::id_t OTHER  = 6;

tooltip_t buttonTooltip() { return { "Measure", { 0.0F, 100.0F, 40.0F, 40.0F } }; }

tooltip_t contentTooltip() { return { "Vertex", { 300.0F, 200.0F, 8.0F, 8.0F } }; }

} // namespace

TEST(SpentButtonOfTest, StaysSpentWhileThePointerMovesOnIt)
{
    EXPECT_EQ(spentButtonOf(BUTTON, buttonTooltip().anchor, 20.0F, 120.0F), BUTTON);
}

TEST(SpentButtonOfTest, AMoveOffItRearmsIt)
{
    EXPECT_EQ(spentButtonOf(BUTTON, buttonTooltip().anchor, 20.0F, 160.0F), INVALID_ID);
    EXPECT_EQ(spentButtonOf(BUTTON, buttonTooltip().anchor, 300.0F, 200.0F), INVALID_ID);
}

// Back on it after leaving it is not spent: once rearmed, nothing spends it but a press
TEST(SpentButtonOfTest, BackOnItAfterLeavingItIsNotSpent)
{
    Ui::id_t spent = spentButtonOf(BUTTON, buttonTooltip().anchor, 300.0F, 200.0F);
    spent          = spentButtonOf(spent, buttonTooltip().anchor, 20.0F, 120.0F);
    EXPECT_EQ(spent, INVALID_ID);
}

TEST(SpentButtonOfTest, NoneStaysNone)
{
    EXPECT_EQ(spentButtonOf(INVALID_ID, buttonTooltip().anchor, 20.0F, 120.0F), INVALID_ID);
}

TEST(TooltipWantedOfTest, ContentTooltipWithNoButtonHovered)
{
    EXPECT_EQ(tooltipWantedOf(buttonTooltip(), INVALID_ID, INVALID_ID, contentTooltip()), contentTooltip());
}

TEST(TooltipWantedOfTest, HoveredButtonTooltipComesFirst)
{
    EXPECT_EQ(tooltipWantedOf(buttonTooltip(), BUTTON, INVALID_ID, contentTooltip()), buttonTooltip());
}

TEST(TooltipWantedOfTest, SpentButtonShowsNoneNotTheContentTooltip)
{
    EXPECT_EQ(tooltipWantedOf(buttonTooltip(), BUTTON, BUTTON, contentTooltip()), tooltip_t {});
}

TEST(TooltipWantedOfTest, AnotherButtonSpentDoesNotSilenceThisOne)
{
    EXPECT_EQ(tooltipWantedOf(buttonTooltip(), BUTTON, OTHER, contentTooltip()), buttonTooltip());
}

TEST(TooltipWantedOfTest, NoneWhenNothingAsks)
{
    EXPECT_EQ(tooltipWantedOf({}, INVALID_ID, INVALID_ID, {}), tooltip_t {});
}

// A button with no tooltip text: none, anchor and all, so it equals the none shown
// and closes nothing again on every sync - and not the content surface's either
TEST(TooltipWantedOfTest, ButtonWithoutTextIsNone)
{
    const tooltip_t untitled = { "", buttonTooltip().anchor };
    EXPECT_EQ(tooltipWantedOf(untitled, BUTTON, INVALID_ID, contentTooltip()), tooltip_t {});
}

TEST(TooltipWantedOfTest, ContentWithoutTextIsNone)
{
    const tooltip_t untitled = { "", contentTooltip().anchor };
    EXPECT_EQ(tooltipWantedOf(buttonTooltip(), INVALID_ID, INVALID_ID, untitled), tooltip_t {});
}

TEST(IsTooltipDelayedTest, TheFirstToolbarTooltipWaits) { EXPECT_TRUE(isTooltipDelayed(buttonTooltip(), BUTTON, {})); }

TEST(IsTooltipDelayedTest, WithOneUpTheNextFollowsAtOnce)
{
    EXPECT_FALSE(isTooltipDelayed(buttonTooltip(), BUTTON, contentTooltip()));
    EXPECT_FALSE(isTooltipDelayed(buttonTooltip(), BUTTON, buttonTooltip()));
}

TEST(IsTooltipDelayedTest, ContentTooltipNeverWaits)
{
    EXPECT_FALSE(isTooltipDelayed(contentTooltip(), INVALID_ID, {}));
}

TEST(IsTooltipDelayedTest, NothingWantedNeverWaits) { EXPECT_FALSE(isTooltipDelayed({}, BUTTON, {})); }
