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
 * @file TestDockStore.cpp
 * @brief Unit tests for Ui::Res::Store::DockStore - session-persisted per-dock
 *        state keyed by dock name. Covers the default (collapsed) state, the
 *        set/get commit, the write/read JSON round trip, the orphan-drop rule
 *        (state for a dock absent from the layout is not written), the
 *        non-array-input guard, and sanitizeDockWidth clamping.
 */

#include "nlohmann/json.hpp"
#include "ui/res/dock/config.h"
#include "ui/res/dock/state.h"
#include "ui/res/store/dockstore.h"
#include "ui/res/store/layoutstore.h"

#include <cmath>
#include <gtest/gtest.h>
#include <limits>
#include <string>
#include <vector>

using Ui::Res::Dock::dock_config_t;
using Ui::Res::Dock::dock_state_t;
using Ui::Res::Store::DockStore;
using Ui::Res::Store::LayoutStore;

namespace {
// A LayoutStore carrying one dock definition, so DockStore has a config to
// filter against on write.
LayoutStore layoutWithDock(const std::string & name)
{
    LayoutStore   layout;
    dock_config_t cfg;
    cfg.name         = name;
    cfg.order        = 1;
    cfg.defaultWidth = 240.0F;
    std::vector<dock_config_t> docks { cfg };
    (void)layout.setDocks(docks);
    return layout;
}
} // namespace

TEST(DockStore, UnknownDockIsCollapsedByDefault)
{
    const LayoutStore  layout;
    const DockStore    store(layout);
    const dock_state_t state = store.dockState("never-set");
    EXPECT_FLOAT_EQ(state.width, 0.0F);
    EXPECT_FLOAT_EQ(state.memoryX, 0.0F);
}

TEST(DockStore, SetThenGet)
{
    const LayoutStore layout;
    const DockStore   store(layout);
    dock_state_t      state;
    state.width   = 200.0F;
    state.memoryX = 180.0F;
    store.setDockState("featureTree", state);

    const dock_state_t got = store.dockState("featureTree");
    EXPECT_FLOAT_EQ(got.width, 200.0F);
    EXPECT_FLOAT_EQ(got.memoryX, 180.0F);
}

TEST(DockStore, WriteReadJsonRoundTrip)
{
    const LayoutStore layout = layoutWithDock("featureTree");
    const DockStore   writer(layout);
    dock_state_t      state;
    state.width   = 260.0F;
    state.memoryX = 240.0F;
    writer.setDockState("featureTree", state);

    nlohmann::json arr = nlohmann::json::array();
    writer.writeDockJson(arr);
    ASSERT_EQ(arr.size(), 1U);

    DockStore reader(layout);
    reader.readDockJson(arr);
    const dock_state_t got = reader.dockState("featureTree");
    EXPECT_FLOAT_EQ(got.width, 260.0F);
    EXPECT_FLOAT_EQ(got.memoryX, 240.0F);
}

TEST(DockStore, OrphanStateNotWritten)
{
    const LayoutStore layout = layoutWithDock("featureTree");
    const DockStore   store(layout);
    // "ghost" is not in the layout config, so it must be dropped on write.
    store.setDockState("featureTree", dock_state_t { 100.0F, 100.0F });
    store.setDockState("ghost", dock_state_t { 50.0F, 50.0F });

    nlohmann::json arr = nlohmann::json::array();
    store.writeDockJson(arr);
    EXPECT_EQ(arr.size(), 1U); // only featureTree
}

TEST(DockStore, ReadNonArrayClearsState)
{
    const LayoutStore layout = layoutWithDock("featureTree");
    DockStore         store(layout);
    store.setDockState("featureTree", dock_state_t { 300.0F, 300.0F });

    store.readDockJson(nlohmann::json::object()); // not an array -> clears
    EXPECT_FLOAT_EQ(store.dockState("featureTree").width, 0.0F);
}

TEST(DockStore, SanitizeDockWidthClampsInvalid)
{
    EXPECT_FLOAT_EQ(DockStore::sanitizeDockWidth(-1.0), 0.0F);
    EXPECT_FLOAT_EQ(DockStore::sanitizeDockWidth(std::nan("")), 0.0F);
    EXPECT_FLOAT_EQ(DockStore::sanitizeDockWidth(std::numeric_limits<double>::infinity()), 0.0F);
    EXPECT_FLOAT_EQ(DockStore::sanitizeDockWidth(150.0), 150.0F);
    EXPECT_FLOAT_EQ(DockStore::sanitizeDockWidth(0.0), 0.0F);
}
