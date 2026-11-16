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
 *        (inside the chrome, less its margin), that projecting dock rows marks
 *        the layout for rebuild, and that each dock's rows reach it by its name,
 *        outlive a layout rebuild, and are asked of every dock.
 *
 * The second one is the bug that made a dock's rows clickable at the positions
 * they held BEFORE the projection: rows are drawn live, but hit rects come from
 * the element list, and only a content refresh rebuilds it.
 */

#include "testconst.h"
#include "testidkind.h"
#include "ui/idkind.h"
#include "ui/render/uielement.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/rowkind.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/changed.h"
#include "ui/window/windowmanager.h"

#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

class WindowGeometryTest : public ::testing::Test {
protected:
    Ui::Res::ResManager       resManager { TEST_RES_DIR };
    Ui::Window::WindowManager windowManager { resManager };

    void SetUp() override
    {
        resManager.loadAll();
        // Docks come from the layout, and only a resource apply builds them - the
        // rest of that path needs no window
        windowManager.apply(Ui::Res::Type::changes_t(Ui::Res::Type::Changed::Layout));
        // No window is ever created: this only stores the size the geometry
        // below is derived from
        windowManager.onMainWindowResize(WINDOW_W, WINDOW_H);
    }

    [[nodiscard]] Ui::fpx_t phys(Ui::fpx_t css) const { return Ui::toPhys(css); }

    // One Menu row, `serial` its id's, opening `menu`
    [[nodiscard]] static std::vector<Ui::Res::Dock::row_t> menuRowOf(Ui::id_t serial, const std::string & menu)
    {
        Ui::Res::Dock::row_t row;
        row.id   = Ui::idOf(TestIdKind::Row, serial);
        row.kind = Ui::Res::Dock::RowKind::Menu;
        row.menu = menu;
        return { row };
    }
};

// ============================================================================
// Where a dialog may sit
// ============================================================================

TEST_F(WindowGeometryTest, DialogAreaStartsInsideTheChrome)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();

    EXPECT_GE(area.x, layout.leftToolbar.width);
    EXPECT_GE(area.y, layout.topMenu.height + layout.workspaceTab.height);
}

TEST_F(WindowGeometryTest, DialogAreaLeavesTheStatusBarAndRightToolbarFree)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();

    EXPECT_LE(area.x + area.w, Ui::toCss(WINDOW_W) - layout.rightToolbar.width);
    EXPECT_LE(area.y + area.h, Ui::toCss(WINDOW_H) - layout.statusBar.height);
}

TEST_F(WindowGeometryTest, DialogAreaIsInsetByTheDialogMarginOnEverySide)
{
    const auto & layout = resManager.layout();
    const auto   area   = windowManager.dialogArea();
    const auto   margin = layout.dialog.margin;

    EXPECT_FLOAT_EQ(area.x, layout.leftToolbar.width + margin);
    EXPECT_FLOAT_EQ(area.y, layout.topMenu.height + layout.workspaceTab.height + margin);
    EXPECT_FLOAT_EQ(area.x + area.w, Ui::toCss(WINDOW_W) - layout.rightToolbar.width - margin);
    EXPECT_FLOAT_EQ(area.y + area.h, Ui::toCss(WINDOW_H) - layout.statusBar.height - margin);
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
    row.id    = Ui::idOf(TestIdKind::Row, 1);
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

// ============================================================================
// Each dock reached by its name, every dock asked
// ============================================================================

// Rows projected by a dock's name reach that dock, and a row's menu is found whichever
// dock holds it
TEST_F(WindowGeometryTest, RowsReachTheDockTheyAreProjectedInto)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_GE(docks.size(), 2U) << "the shipped layout declares fewer than two docks to tell apart";
    for (std::size_t index = 0; index < docks.size(); ++index) {
        windowManager.setDockRows(docks.at(index).name, menuRowOf(index + 1, "menu-of-" + docks.at(index).name));
    }
    for (std::size_t index = 0; index < docks.size(); ++index) {
        EXPECT_EQ(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, index + 1)), "menu-of-" + docks.at(index).name)
        << docks.at(index).name;
    }
}

// Rows a host projected outlive a layout rebuild: each dock is made again and takes
// back its own by name
TEST_F(WindowGeometryTest, RowsOutliveALayoutRebuild)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_FALSE(docks.empty());
    windowManager.setDockRows(docks.back().name, menuRowOf(7, "kept"));
    windowManager.apply(Ui::Res::Type::changes_t(Ui::Res::Type::Changed::Layout));
    windowManager.apply(Ui::Res::Type::changes_t(Ui::Res::Type::Changed::Layout));
    EXPECT_EQ(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 7)), "kept");
}

// Rows projected again replace the dock's own, the other docks' kept
TEST_F(WindowGeometryTest, RowsProjectedAgainReplaceTheDocksOwn)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_GE(docks.size(), 2U);
    windowManager.setDockRows(docks.front().name, menuRowOf(1, "first"));
    windowManager.setDockRows(docks.back().name, menuRowOf(2, "second"));
    windowManager.setDockRows(docks.front().name, menuRowOf(3, "again"));
    EXPECT_TRUE(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 1)).empty());
    EXPECT_EQ(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 2)), "second");
    EXPECT_EQ(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 3)), "again");
}

// A dock's scroll reset by its name marks the layout for rebuild, as its rows do
TEST_F(WindowGeometryTest, ResettingADocksScrollMarksTheContentDirty)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_FALSE(docks.empty());
    ASSERT_FALSE(windowManager.isContentDirty());
    windowManager.resetDockScroll(docks.front().name);
    EXPECT_TRUE(windowManager.isContentDirty());
}

// Negative: rows for a dock no layout declares reach no dock
TEST_F(WindowGeometryTest, RowsForAnUnknownDockReachNoDock)
{
    windowManager.setDockRows("no-such-dock", menuRowOf(1, "lost"));
    EXPECT_TRUE(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 1)).empty());
}

// Negative: a row no dock holds, or one that is no Menu row, opens no menu
TEST_F(WindowGeometryTest, OnlyAHeldMenuRowOpensAMenu)
{
    const auto & docks = resManager.layout().docks;
    ASSERT_FALSE(docks.empty());
    std::vector<Ui::Res::Dock::row_t> rows = menuRowOf(1, "a-menu");
    rows.front().kind                      = Ui::Res::Dock::RowKind::Text;
    windowManager.setDockRows(docks.front().name, std::move(rows));
    EXPECT_TRUE(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 1)).empty());
    EXPECT_TRUE(windowManager.rowMenuKey(Ui::idOf(TestIdKind::Row, 99)).empty());
    EXPECT_TRUE(windowManager.rowMenuKey(Ui::INVALID_ID).empty());
}

// Negative: resetting the scroll of a dock no layout declares changes nothing
TEST_F(WindowGeometryTest, ResettingAnUnknownDocksScrollChangesNothing)
{
    ASSERT_FALSE(windowManager.isContentDirty());
    windowManager.resetDockScroll("no-such-dock");
    EXPECT_FALSE(windowManager.isContentDirty());
}

} // namespace PureGlUi
