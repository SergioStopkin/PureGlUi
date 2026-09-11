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
 * @file TestScrollbarRes.cpp
 * @brief Unit tests for the res blocks added with the shared scrollbar: the
 *        "scrollbar" / "scrollbar:hover" geometry and colours (one block for the
 *        dialog and every dock), the "window" minimum, and the dock's
 *        row-key-ratio split. Also pins the key spellings, since a typo in
 *        elementKeyName silently yields defaults rather than an error.
 */

#include "ui/res/key/element.h"
#include "ui/res/respath.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/store/themestore.h"

#include <gtest/gtest.h>
#include <string>

using Ui::Res::ResPath;
using Ui::Res::Key::ElementKey;
using Ui::Res::Key::elementKeyName;
using Ui::Res::Store::LayoutStore;
using Ui::Res::Store::ThemeStore;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }
} // namespace

// ============================================================================
// Key spellings - the selector both stores agree on
// ============================================================================

TEST(ScrollbarRes, KeysAreSpelledWithoutTheDialogPrefix)
{
    EXPECT_EQ(elementKeyName(ElementKey::Scrollbar), "scrollbar");
    EXPECT_EQ(elementKeyName(ElementKey::ScrollbarHover), "scrollbar:hover");
    EXPECT_EQ(elementKeyName(ElementKey::ScrollbarThumb), "scrollbar-thumb");
    EXPECT_EQ(elementKeyName(ElementKey::ScrollbarThumbHover), "scrollbar-thumb:hover");
    EXPECT_EQ(elementKeyName(ElementKey::Window), "window");
}

// ============================================================================
// Layout: geometry for both states
// ============================================================================

TEST(ScrollbarRes, ShippedLayoutDefinesBothScrollbarStates)
{
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    const auto & layout = store.layout();

    EXPECT_GT(layout.scrollbarW, 0.0F);
    EXPECT_GT(layout.scrollbarHoverW, 0.0F);
    EXPECT_GT(layout.scrollbarMinThumb, 0.0F);
    EXPECT_GT(layout.scrollbarHoverMinThumb, 0.0F);
}

// The hover state is what the hit area uses, so it must not be the narrower one
TEST(ScrollbarRes, HoverWidthIsNeverNarrowerThanIdle)
{
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    EXPECT_GE(store.layout().scrollbarHoverW, store.layout().scrollbarW);
}

TEST(ScrollbarRes, WindowMinimumIsParsed)
{
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    EXPECT_GT(store.layout().windowMinWidth, 0.0F);
    EXPECT_GT(store.layout().windowMinHeight, 0.0F);
}

TEST(ScrollbarRes, RowKeyRatioIsAFractionOfTheRow)
{
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    const Ui::fpx_t ratio = store.layout().dockDefaults.rowKeyRatio;
    EXPECT_GT(ratio, 0.0F);
    EXPECT_LT(ratio, 1.0F);
}

// Negative: an absent file leaves every new field at its zero default rather
// than at some half-parsed value
TEST(ScrollbarRes, MissingLayoutFileLeavesNewFieldsAtDefaults)
{
    LayoutStore store;
    (void)store.load(std::string(TEST_RES_DIR) + "/no-such-layout.json");
    const auto & layout = store.layout();
    EXPECT_FLOAT_EQ(layout.scrollbarW, 0.0F);
    EXPECT_FLOAT_EQ(layout.scrollbarHoverW, 0.0F);
    EXPECT_FLOAT_EQ(layout.windowMinWidth, 0.0F);
    EXPECT_FLOAT_EQ(layout.windowMinHeight, 0.0F);
}

// ============================================================================
// Theme: colours for track, thumb and hovered thumb
// ============================================================================

TEST(ScrollbarRes, ShippedThemeDefinesScrollbarColours)
{
    ThemeStore store;
    (void)store.loadCurrent(resPath());
    const auto & theme = store.theme();

    EXPECT_GT(theme.scrollbarTrack.a(), 0);
    EXPECT_GT(theme.scrollbarThumb.a(), 0);
    EXPECT_GT(theme.scrollbarThumbHover.a(), 0);
}

TEST(ScrollbarRes, HoveredThumbIsAColourOfItsOwn)
{
    ThemeStore store;
    (void)store.loadCurrent(resPath());
    EXPECT_NE(store.theme().scrollbarThumb, store.theme().scrollbarThumbHover);
}
