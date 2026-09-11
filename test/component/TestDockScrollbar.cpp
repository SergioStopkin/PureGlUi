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
 * @file TestDockScrollbar.cpp
 * @brief Tests for the dock's half of the shared scrollbar: when it exists at
 *        all (a collapsed dock has no content area, so a bar hung off it would
 *        land on the grip), which element it contributes to hit-testing, and
 *        that its gestures move the row window.
 *
 * A real ResManager over the shipped res/ supplies the dock defaults and the
 * "scrollbar" block; dock state is seeded per test so nothing leaks between them.
 */

#include "ui/render/dockcolumn.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/state.h"
#include "ui/res/resmanager.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

using Ui::fpx_t;
using Ui::Render::DockColumn;
using Ui::Render::UiElementType;
using Ui::Render::UiLayout;
using Ui::Res::ResManager;
using Ui::Res::Dock::dock_config_t;
using Ui::Res::Dock::dock_state_t;
using Ui::Res::Dock::DockAnchor;
using Ui::Res::Dock::row_t;

namespace {

constexpr fpx_t DOCK_TOP    = 100.0F;
constexpr fpx_t DOCK_HEIGHT = 200.0F;
constexpr fpx_t DOCK_EDGE   = 300.0F; // viewport-facing edge of a left dock
constexpr fpx_t DOCK_WIDTH  = 180.0F;

class DockScrollbarTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_resManager = std::make_unique<ResManager>(PROJECT_RES_DIR);
        s_resManager->loadAll();
    }

    static void TearDownTestSuite() { s_resManager.reset(); }

    // Seed a uniquely-named dock's committed width, so tests never collide
    // through the shared ResManager or a persisted session
    static dock_config_t seed(const std::string & name, fpx_t width)
    {
        s_resManager->setDockState(name, dock_state_t { width, width });
        return dock_config_t { name, DockAnchor::Left, 1, width };
    }

    static std::vector<row_t> makeRows(std::size_t count)
    {
        std::vector<row_t> rows;
        rows.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            row_t row;
            row.id    = static_cast<Ui::id_t>(i + 1);
            row.label = "row" + std::to_string(i);
            rows.emplace_back(std::move(row));
        }
        return rows;
    }

    // Rows guaranteed to overflow the dock, whatever the row height is
    static std::size_t overflowingRowCount()
    {
        const fpx_t rowH = s_resManager->popup().itemHeight;
        return static_cast<std::size_t>(DOCK_HEIGHT / rowH) + 10;
    }

    static std::size_t countOf(const UiLayout & layout, UiElementType type)
    {
        std::size_t count = 0;
        for (const auto & element : layout.elements()) {
            if (element.type == type) {
                ++count;
            }
        }
        return count;
    }

    static std::unique_ptr<ResManager> s_resManager;
};

std::unique_ptr<ResManager> DockScrollbarTest::s_resManager;

} // namespace

// ============================================================================
// When the bar exists
// ============================================================================

TEST_F(DockScrollbarTest, FewRowsNeedNoScrollbar)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sb-few", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(1));

    dock.appendElements(layout);
    EXPECT_EQ(countOf(layout, UiElementType::DockScrollbar), 0U);
}

TEST_F(DockScrollbarTest, OverflowingRowsContributeExactlyOneScrollbarElement)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sb-many", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    dock.appendElements(layout);
    EXPECT_EQ(countOf(layout, UiElementType::DockScrollbar), 1U);
}

// Negative and the reason isScrollable gained its width test: a collapsed dock
// has no content area, so a bar would be drawn outside it - over the grip
TEST_F(DockScrollbarTest, CollapsedDockHasNoScrollbarHoweverManyRows)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sb-collapsed", 0.0F), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    dock.appendElements(layout);
    EXPECT_EQ(countOf(layout, UiElementType::DockScrollbar), 0U);
    // The grip is still there - it is the only way to bring the dock back
    EXPECT_EQ(countOf(layout, UiElementType::DockGrip), 1U);
}

TEST_F(DockScrollbarTest, ScrollbarSitsInsideTheDockAndSpansItsContent)
{
    UiLayout   layout;
    DockColumn dock(1, seed("sb-bounds", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    dock.appendElements(layout);
    for (const auto & element : layout.elements()) {
        if (element.type != UiElementType::DockScrollbar) {
            continue;
        }
        EXPECT_GE(element.bound.x, dock.outer().x);
        EXPECT_LE(element.bound.x + element.bound.w, dock.outer().x + dock.outer().w);
        EXPECT_FLOAT_EQ(element.bound.y, DOCK_TOP);
        EXPECT_FLOAT_EQ(element.bound.h, DOCK_HEIGHT);
    }
}

// ============================================================================
// Gestures
// ============================================================================

TEST_F(DockScrollbarTest, PressOnTheTrackBelowTheThumbPagesDown)
{
    DockColumn dock(1, seed("sb-page", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));
    ASSERT_EQ(dock.firstRow(), 0U);

    dock.beginScrollGesture(DOCK_TOP + DOCK_HEIGHT - 1.0F);
    EXPECT_FALSE(dock.isDraggingScroll());
    EXPECT_GT(dock.firstRow(), 0U);
}

TEST_F(DockScrollbarTest, PressOnTheThumbGrabsInsteadOfPaging)
{
    DockColumn dock(1, seed("sb-grab", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    dock.beginScrollGesture(DOCK_TOP + 1.0F); // the thumb is at the top at row 0
    EXPECT_TRUE(dock.isDraggingScroll());
    EXPECT_EQ(dock.firstRow(), 0U);

    dock.endScrollDrag();
    EXPECT_FALSE(dock.isDraggingScroll());
}

TEST_F(DockScrollbarTest, DraggingTheThumbMovesTheRowWindowAndStopsAtTheEnd)
{
    DockColumn dock(1, seed("sb-drag", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    dock.beginScrollGesture(DOCK_TOP + 1.0F);
    ASSERT_TRUE(dock.isDraggingScroll());

    EXPECT_TRUE(dock.onScrollDrag(DOCK_TOP + (DOCK_HEIGHT / 2.0F)));
    const std::size_t middle = dock.firstRow();
    EXPECT_GT(middle, 0U);

    // Past the bottom: clamped, and a second identical drag reports no change
    dock.onScrollDrag(DOCK_TOP + DOCK_HEIGHT + 1000.0F);
    const std::size_t last = dock.firstRow();
    EXPECT_GT(last, middle);
    EXPECT_FALSE(dock.onScrollDrag(DOCK_TOP + DOCK_HEIGHT + 2000.0F));
}

// Negative: a dock with nothing to scroll must ignore the gesture rather than
// divide by a zero range
TEST_F(DockScrollbarTest, GestureOnANonScrollableDockIsInert)
{
    DockColumn dock(1, seed("sb-inert", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(1));

    dock.beginScrollGesture(DOCK_TOP + DOCK_HEIGHT - 1.0F);
    EXPECT_FALSE(dock.isDraggingScroll());
    EXPECT_EQ(dock.firstRow(), 0U);
}

// ============================================================================
// Hover, which is what widens the bar
// ============================================================================

TEST_F(DockScrollbarTest, HoverOverTheBarReportsAChangeOnceAndClearsOnLeave)
{
    DockColumn dock(1, seed("sb-hover", DOCK_WIDTH), *s_resManager);
    dock.setLayout(DOCK_TOP, DOCK_HEIGHT, DOCK_EDGE);
    dock.setRows(makeRows(overflowingRowCount()));

    // The bar sits at the content's right edge, just left of the grip
    const fpx_t barX = dock.content().x + dock.content().w - 2.0F;
    const fpx_t barY = DOCK_TOP + (DOCK_HEIGHT / 2.0F);

    EXPECT_TRUE(dock.onMouseMove(barX, barY));
    EXPECT_FALSE(dock.onMouseMove(barX, barY));

    // Far from the bar: the hover state drops again
    EXPECT_TRUE(dock.onMouseMove(dock.content().x + 1.0F, barY));
}
