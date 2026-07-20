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
 * @file TestLayoutStore.cpp
 * @brief Unit tests for Ui::Res::Store::LayoutStore. Loads the shipped
 *        res/layout.json (TEST_RES_DIR), verifies the derived popup metrics and
 *        geometry, the Changed-bit semantics (a re-load with identical values is
 *        a no-op), the CSS-default fallback when the file is absent, and the
 *        setDocks change detection.
 */

#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/respath.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/type/changed.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using Ui::Res::ResPath;
using Ui::Res::Dock::dock_config_t;
using Ui::Res::Store::LayoutStore;
using Ui::Res::Type::Changed;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }
} // namespace

TEST(LayoutStore, LoadShippedLayoutChangesFromDefault)
{
    LayoutStore   store;
    const Changed changed = store.load(resPath().layoutFile());
    // The shipped file differs from the zero-initialized default, so at least
    // one of the Layout / Popup bits must be set.
    EXPECT_NE(changed, Changed::None);
}

TEST(LayoutStore, PopupMetricsDerivedFromLayout)
{
    LayoutStore store;
    store.load(resPath().layoutFile());
    const auto & popup = store.popup();
    // itemHeight = fontSize * lineHeight + paddingV * 2, always > 0 once parsed.
    EXPECT_GT(popup.itemHeight, 0.0F);
    EXPECT_GT(popup.itemPaddingV, 0.0F);
    EXPECT_GT(popup.separatorHeight, 0.0F);
}

TEST(LayoutStore, TopMenuRegionParsed)
{
    LayoutStore store;
    store.load(resPath().layoutFile());
    EXPECT_GT(store.layout().topMenu.height, 0.0F);
}

TEST(LayoutStore, ReloadIdenticalIsNoOp)
{
    LayoutStore store;
    store.load(resPath().layoutFile());
    // Second identical load parses the same values -> nothing changed.
    EXPECT_EQ(store.load(resPath().layoutFile()), Changed::None);
}

TEST(LayoutStore, MissingFileAppliesPopupDefaults)
{
    LayoutStore store;
    // A path that does not exist -> loadPopupFrom(nullptr) applies CSS defaults.
    const Changed changed = store.load(std::string(TEST_RES_DIR) + "/does-not-exist.json");
    EXPECT_EQ(changed, Changed::Popup);
    // Defaults: fontSize 14 * lineHeight 1.38 + paddingV 6 * 2 = 31.32
    EXPECT_NEAR(store.popup().itemHeight, 14.0F * 1.38F + 6.0F * 2.0F, 0.01F);
    EXPECT_FLOAT_EQ(store.popup().itemPaddingV, 6.0F);
    EXPECT_FLOAT_EQ(store.popup().itemPaddingH, 16.0F);
    EXPECT_FLOAT_EQ(store.popup().separatorHeight, 1.0F);
    EXPECT_FLOAT_EQ(store.popup().separatorMarginV, 4.0F);
    EXPECT_FLOAT_EQ(store.popup().separatorMarginH, 0.0F);
}

TEST(LayoutStore, SetDocksChangeDetection)
{
    LayoutStore store;
    // Empty -> empty (the default) is a no-op.
    EXPECT_EQ(store.setDocks({}), Changed::None);

    std::vector<dock_config_t> docks;
    dock_config_t              dock;
    dock.name         = "featureTree";
    dock.order        = 1;
    dock.defaultWidth = 240.0F;
    docks.emplace_back(dock);

    EXPECT_EQ(store.setDocks(docks), Changed::Layout); // first set changes
    EXPECT_EQ(store.setDocks(docks), Changed::None);   // identical set is a no-op
    EXPECT_EQ(store.setDocks({}), Changed::Layout);    // clearing changes again
}
