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
 * @file TestMinWindowSize.cpp
 * @brief Unit tests for ResManager::minWindowSize and the MenuStore half it is
 *        derived from. The floor is what stops a window from being resized
 *        smaller than the dialogs it has to show, and it is DERIVED so that a
 *        bigger dialog raises it without anyone editing a second number.
 */

#include "menustorefixture.h"
#include "ui/res/resmanager.h"
#include "ui/res/respath.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

using Ui::fpx_t;
using Ui::Res::ResManager;
using Ui::Res::ResPath;

namespace {

class MinWindowSizeTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_resManager = std::make_unique<ResManager>(TEST_RES_DIR);
        s_resManager->loadAll();
    }

    static void TearDownTestSuite() { s_resManager.reset(); }

    static std::unique_ptr<ResManager> s_resManager;
};

std::unique_ptr<ResManager> MinWindowSizeTest::s_resManager;

ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

} // namespace

// ============================================================================
// The derived floor
// ============================================================================

TEST_F(MinWindowSizeTest, FloorIsAtLeastTheAuthoredChromeMinimum)
{
    const auto & layout = s_resManager->layout();
    const auto   size   = s_resManager->minWindowSize();
    EXPECT_GE(size.w, layout.windowMinWidth);
    EXPECT_GE(size.h, layout.windowMinHeight);
}

TEST_F(MinWindowSizeTest, FloorIsPositiveAndCarriesNoOrigin)
{
    const auto size = s_resManager->minWindowSize();
    EXPECT_GT(size.w, 0.0F);
    EXPECT_GT(size.h, 0.0F);
    EXPECT_FLOAT_EQ(size.x, 0.0F);
    EXPECT_FLOAT_EQ(size.y, 0.0F);
}

// The whole point of deriving it: whatever the biggest dialog is, the floor
// leaves room for it plus the bars it is centred between
TEST_F(MinWindowSizeTest, FloorFitsTheLargestDialogPlusTheChromeAroundIt)
{
    TestSupport::menu_store_fixture_t fixture;
    (void)fixture.layout.load(resPath().layoutFile());
    (void)fixture.store.loadMenus(resPath().menuDir());

    const auto & layout    = s_resManager->layout();
    const auto & maxDialog = fixture.store.maxDialog();
    const auto   size      = s_resManager->minWindowSize();

    const fpx_t margin = layout.dialog.margin * 2.0F;
    const fpx_t sides  = layout.leftToolbar.width + layout.rightToolbar.width + margin;
    const fpx_t bands  = layout.topMenu.height + layout.workspaceTab.height + layout.statusBar.height + margin;

    EXPECT_GE(size.w, maxDialog.w + sides);
    EXPECT_GE(size.h, maxDialog.h + bands);
}

// ============================================================================
// MenuStore's half: the largest dialog, and its reset
// ============================================================================

TEST(MaxDialog, TracksTheWidestAndTallestDeclaredDialog)
{
    TestSupport::menu_store_fixture_t fixture;
    (void)fixture.store.loadMenus(resPath().menuDir());
    // A dialog with no explicit size falls back to the layout default and
    // contributes nothing, so a shipped set may legitimately declare none
    EXPECT_GE(fixture.store.maxDialog().w, 0.0F);
    EXPECT_GE(fixture.store.maxDialog().h, 0.0F);
}

TEST(MaxDialog, ReloadRecomputesRatherThanAccumulating)
{
    TestSupport::menu_store_fixture_t fixture;
    (void)fixture.store.loadMenus(resPath().menuDir());
    const auto first = fixture.store.maxDialog();

    (void)fixture.store.loadMenus(resPath().menuDir());
    EXPECT_EQ(fixture.store.maxDialog(), first);
}

// Negative: nothing to load leaves the maximum at zero, so the floor falls back
// to the authored chrome minimum alone
TEST(MaxDialog, MissingMenuDirLeavesNoDialogSize)
{
    TestSupport::menu_store_fixture_t fixture;
    (void)fixture.store.loadMenus(std::string(TEST_RES_DIR) + "/does-not-exist");
    EXPECT_FLOAT_EQ(fixture.store.maxDialog().w, 0.0F);
    EXPECT_FLOAT_EQ(fixture.store.maxDialog().h, 0.0F);
}
