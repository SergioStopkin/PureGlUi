// Copyright © 2025-2026 Sergio Stopkin.

/*
 * This file is part of PureCreator. PureCreator is free software:
 * you can redistribute it and/or modify it under the terms of the
 * GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * PureCreator is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with PureCreator. See the file COPYING. If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * @file TestDockColumn.cpp
 * @brief Unit tests for DockColumn::clampStateWidth.
 *
 * Covers the double-click-restore safety net added after the "two-docks-
 * intercept" gap: when applyDoubleClickToggle restores width from memoryX,
 * WindowManager must clamp the committed width against current viewport
 * availability so the freshly-revealed dock cannot overlap others or push
 * the OCCT viewport below zero.
 *
 * Each test bootstraps a real ResManager pointed at the project's res/
 * directory (PROJECT_RES_DIR). Dock state is overridden per test via
 * setDockState to avoid cross-test pollution from a persisted session.
 */

#include "ui/render/dockcolumn.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/state.h"
#include "ui/res/resmanager.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>

using Ui::fpx_t;
using Ui::Render::DockColumn;
using Ui::Res::ResManager;
using Ui::Res::Dock::dock_config_t;
using Ui::Res::Dock::dock_state_t;
using Ui::Res::Dock::DockAnchor;

namespace {

// Shared ResManager for the suite, loaded from the real res/ so dock
// defaults (gripWidth, clickThreshold) are realistic.
//
// No SessionManager is constructed here, so setDockState's persist hook stays
// the default no-op - every setDockState is kept in memory and tests never
// write the developer's .purecreator/session.json.
class DockColumnTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        s_resManager = std::make_unique<ResManager>(PROJECT_RES_DIR);
        s_resManager->loadAll();
    }

    static void TearDownTestSuite() { s_resManager.reset(); }

    // Seed a known starting state for a uniquely-named dock so concurrent
    // tests don't trip over each other through the shared resmanager.
    static void seed(const std::string & name, dock_state_t state) { s_resManager->setDockState(name, state); }

    static dock_state_t readState(const std::string & name) { return s_resManager->dockState(name); }

    static fpx_t gripWidth() { return s_resManager->layout().dockDefaults.gripWidth; }

    static std::unique_ptr<ResManager> s_resManager;
};

std::unique_ptr<ResManager> DockColumnTest::s_resManager;

// Helper that mirrors a "left-anchored test dock with no committed width
// and a custom defaultWidth/memoryX" - used as the common case below.
dock_config_t makeConfig(const std::string & name, fpx_t defaultWidth)
{
    return dock_config_t { name, DockAnchor::Left, /*order*/ 1, defaultWidth };
}

} // namespace

// ============================================================================
// clampStateWidth - the new method added for the double-click-restore fix
// ============================================================================

TEST_F(DockColumnTest, ClampState_AboveMax_Clamps)
{
    seed("clamp-above", { 500.0F, 500.0F });
    const dock_config_t cfg = makeConfig("clamp-above", 200.0F);
    DockColumn          dock { /*id*/ 1, cfg, *s_resManager };

    dock.clampStateWidth(240.0F);

    EXPECT_FLOAT_EQ(dock.currentWidth(), 240.0F + gripWidth());
    EXPECT_FLOAT_EQ(readState("clamp-above").width, 240.0F);
}

TEST_F(DockColumnTest, ClampState_BelowMax_Unchanged)
{
    seed("clamp-below", { 100.0F, 100.0F });
    const dock_config_t cfg = makeConfig("clamp-below", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(240.0F);

    EXPECT_FLOAT_EQ(dock.currentWidth(), 100.0F + gripWidth());
    EXPECT_FLOAT_EQ(readState("clamp-below").width, 100.0F);
}

TEST_F(DockColumnTest, ClampState_EqualToMax_Unchanged)
{
    seed("clamp-equal", { 240.0F, 240.0F });
    const dock_config_t cfg = makeConfig("clamp-equal", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(240.0F);

    EXPECT_FLOAT_EQ(readState("clamp-equal").width, 240.0F);
}

TEST_F(DockColumnTest, ClampState_ClampsMemoryXToo)
{
    // memoryX is capped alongside width so session.json never persists a
    // "restore size" the viewport can't actually accommodate.
    seed("clamp-memory", { 500.0F, /*memoryX*/ 500.0F });
    const dock_config_t cfg = makeConfig("clamp-memory", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(240.0F);

    const dock_state_t after = readState("clamp-memory");
    EXPECT_FLOAT_EQ(after.width, 240.0F);
    EXPECT_FLOAT_EQ(after.memoryX, 240.0F);
}

TEST_F(DockColumnTest, ClampState_LeavesSmallerMemoryXAlone)
{
    // memoryX < clamped means the stored restore size is already valid;
    // don't grow it back up.
    seed("clamp-memory-small", { 500.0F, /*memoryX*/ 120.0F });
    const dock_config_t cfg = makeConfig("clamp-memory-small", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(240.0F);

    const dock_state_t after = readState("clamp-memory-small");
    EXPECT_FLOAT_EQ(after.width, 240.0F);
    EXPECT_FLOAT_EQ(after.memoryX, 120.0F);
}

TEST_F(DockColumnTest, ClampState_MaxZero_CollapsesContent)
{
    seed("clamp-zero", { 500.0F, 500.0F });
    const dock_config_t cfg = makeConfig("clamp-zero", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(0.0F);

    EXPECT_FLOAT_EQ(dock.currentWidth(), gripWidth()); // content collapsed
    EXPECT_FLOAT_EQ(readState("clamp-zero").width, 0.0F);
}

TEST_F(DockColumnTest, ClampState_NegativeMax_TreatedAsZero)
{
    seed("clamp-negative", { 500.0F, 500.0F });
    const dock_config_t cfg = makeConfig("clamp-negative", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(-50.0F);

    EXPECT_FLOAT_EQ(readState("clamp-negative").width, 0.0F);
}

TEST_F(DockColumnTest, ClampState_AlreadyZero_StillClampsMemoryX)
{
    // Collapsed (width=0) but memoryX would restore to a size that no
    // longer fits. memoryX gets capped so the next double-click restore
    // lands at a valid width directly.
    seed("clamp-already-zero", { 0.0F, /*memoryX*/ 300.0F });
    const dock_config_t cfg = makeConfig("clamp-already-zero", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(100.0F);

    const dock_state_t after = readState("clamp-already-zero");
    EXPECT_FLOAT_EQ(after.width, 0.0F);
    EXPECT_FLOAT_EQ(after.memoryX, 100.0F);
}

TEST_F(DockColumnTest, ClampState_DoesNotTouchHoverFlag)
{
    seed("clamp-hover", { 500.0F, 500.0F });
    const dock_config_t cfg = makeConfig("clamp-hover", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    EXPECT_FALSE(dock.isHoveredGrip());
    dock.clampStateWidth(100.0F);
    EXPECT_FALSE(dock.isHoveredGrip());
}

TEST_F(DockColumnTest, ClampState_DoesNotStartDrag)
{
    seed("clamp-drag", { 500.0F, 500.0F });
    const dock_config_t cfg = makeConfig("clamp-drag", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    EXPECT_FALSE(dock.isDragging());
    dock.clampStateWidth(100.0F);
    EXPECT_FALSE(dock.isDragging());
}

// ============================================================================
// Double-click restore scenario reproduced from the user-reported gap
// ============================================================================

TEST_F(DockColumnTest, RepoScenario_RestoreThenClampPreventsOverlap)
{
    // Reproduces the user-reported sequence: a dock collapsed earlier with
    // a generous memoryX is restored via double-click after another dock
    // has consumed most of the workspace. The WindowManager-side clamp is
    // what enforces the no-overlap invariant; we simulate it by calling
    // clampStateWidth with the would-be max.
    //
    // Workspace = 704 CSS (800 - 48 - 48 toolbars).
    // Other dock occupies 457.5 (content 451.5 + grip 6).
    // -> available = 704 - 457.5 = 246.5
    // -> max content for this dock = available - grip = 240.5
    seed("repro-properties", { /*width*/ 498.667F, /*memoryX*/ 498.667F });
    const dock_config_t cfg = makeConfig("repro-properties", 200.0F);
    DockColumn          dock { 1, cfg, *s_resManager };

    dock.clampStateWidth(240.5F);

    const dock_state_t after = readState("repro-properties");
    EXPECT_LE(after.width, 240.5F);
    EXPECT_LE(after.memoryX, 240.5F); // session.json never persists memoryX > current ceiling
}
