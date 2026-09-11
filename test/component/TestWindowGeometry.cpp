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
 * @file TestWindowGeometry.cpp
 * @brief WindowManager geometry that needs no window: where a dialog may sit
 *        (inside the chrome, less its margin), and that projecting dock rows
 *        marks the layout for rebuild.
 *
 * The second one is the bug that made a dock's rows clickable at the positions
 * they held BEFORE the projection: rows are drawn live, but hit rects come from
 * the element list, and only a content refresh rebuilds it.
 */

#include "ui/render/uielement.h"
#include "ui/res/dock/row.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/changed.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

namespace {
    constexpr Ui::fpx_t WINDOW_W = 1600.0F;
    constexpr Ui::fpx_t WINDOW_H = 1000.0F;
} // namespace

class WindowGeometryTest : public ::testing::Test {
protected:
    Ui::Res::ResManager       resManager { TEST_RES_DIR };
    Ui::Window::WindowManager windowManager { resManager };

    void SetUp() override
    {
        resManager.loadAll();
        // Docks come from the layout, and only a resource apply builds them - the
        // rest of that path needs no window
        windowManager.apply(Ui::Res::Type::Changed::Layout);
        // No window is ever created: this only stores the size the geometry
        // below is derived from
        windowManager.onMainWindowResize(WINDOW_W, WINDOW_H);
    }

    [[nodiscard]] Ui::fpx_t phys(Ui::fpx_t css) const { return Ui::toPhys(css); }
};

// ============================================================================
// Where a dialog may sit
// ============================================================================

TEST_F(WindowGeometryTest, DialogAreaStartsInsideTheChrome)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();

    EXPECT_GE(area.x, phys(layout.leftToolbar.width));
    EXPECT_GE(area.y, phys(layout.topMenu.height + layout.workspaceTab.height));
}

TEST_F(WindowGeometryTest, DialogAreaLeavesTheStatusBarAndRightToolbarFree)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();

    EXPECT_LE(area.x + area.w, WINDOW_W - phys(layout.rightToolbar.width));
    EXPECT_LE(area.y + area.h, WINDOW_H - phys(layout.statusBar.height));
}

TEST_F(WindowGeometryTest, DialogAreaIsInsetByTheDialogMarginOnEverySide)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();
    const auto   margin = phys(layout.dialog.margin);

    EXPECT_FLOAT_EQ(area.x, phys(layout.leftToolbar.width) + margin);
    EXPECT_FLOAT_EQ(area.y, phys(layout.topMenu.height + layout.workspaceTab.height) + margin);
    EXPECT_FLOAT_EQ(area.x + area.w, WINDOW_W - phys(layout.rightToolbar.width) - margin);
    EXPECT_FLOAT_EQ(area.y + area.h, WINDOW_H - phys(layout.statusBar.height) - margin);
}

// The floor exists so the largest dialog fits in this area; if the two ever
// disagree, a dialog is unreachable at the smallest allowed window
TEST_F(WindowGeometryTest, TheWindowFloorLeavesRoomForTheLargestDialog)
{
    const auto min = resManager.minWindowSize();
    windowManager.onMainWindowResize(phys(min.w), phys(min.h));

    const auto area = windowManager.dialogArea();
    EXPECT_GT(area.w, 0.0F);
    EXPECT_GT(area.h, 0.0F);
}

// Negative: a window smaller than any chrome would leave a degenerate area, and
// the clamp must keep it positive rather than inverted
TEST_F(WindowGeometryTest, AnImpossiblySmallWindowStillYieldsAPositiveArea)
{
    windowManager.onMainWindowResize(1.0F, 1.0F);
    const auto area = windowManager.dialogArea();
    EXPECT_GT(area.w, 0.0F);
    EXPECT_GT(area.h, 0.0F);
}

// ============================================================================
// Projecting rows marks the layout for rebuild
// ============================================================================

TEST_F(WindowGeometryTest, SettingDockRowsMarksTheContentDirty)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_FALSE(docks.empty()) << "the shipped layout declares no dock to project into";
    // A resize alone moves no element, so the layout is still current
    ASSERT_FALSE(windowManager.isContentDirty());

    std::vector<Ui::Res::Dock::row_t> rows;
    Ui::Res::Dock::row_t              row;
    row.id    = 1;
    row.label = "projected";
    rows.emplace_back(row);
    windowManager.setDockRows(docks.front().name, std::move(rows));

    EXPECT_TRUE(windowManager.isContentDirty());
}

// Negative: an unknown dock name is a no-op, not a rebuild - a host may project
// into a dock its res does not declare
TEST_F(WindowGeometryTest, RowsForAnUnknownDockChangeNothing)
{
    ASSERT_FALSE(windowManager.isContentDirty());

    windowManager.setDockRows("no-such-dock", {});
    EXPECT_FALSE(windowManager.isContentDirty());
}

} // namespace PureGlUi
