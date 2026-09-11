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
 * @file TestSliderRes.cpp
 * @brief Unit tests for the dock slider's res blocks: "dock-slider-track",
 *        "dock-slider-thumb" and its hover, across both layout and theme.
 *
 * The slider used to be one block described by STATE - "dock-slider" for the
 * track and "dock-slider:active" for the filled portion and thumb together - and
 * carried no geometry at all. It is now described by PART, in both files, so the
 * same name means the same thing whichever one you are reading.
 *
 * Which is exactly why these exist. The rename touched elementKeyName and every
 * shipped theme at once, and the two sides only meet at a string lookup, where a
 * mismatch is not an error. The two sides fail differently, though, and neither
 * failure names itself: layout falls back to 0.0F and the part collapses to
 * nothing, while theme falls back to --cl-error and the part is painted in the
 * error colour. TestScrollbarRes makes the same argument for the block this one
 * was modelled on.
 *
 * The colours come from a fixture, not from res/. This repo's res/ is a demo set
 * that ships no slider colours, so asserting against it would measure the demo
 * rather than the mapping. What is fw-side here is that each block reaches its
 * own pair; that a real theme DEFINES them is a host's contract with its own
 * res/, and is tested there.
 */

#include "ui/res/key/element.h"
#include "ui/res/respath.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/store/themestore.h"
#include "ui/res/type/changed.h"

#include <gtest/gtest.h>
#include <string>

using Ui::Res::ResPath;
using Ui::Res::Key::ElementKey;
using Ui::Res::Key::elementKeyName;
using Ui::Res::Store::LayoutStore;
using Ui::Res::Store::ThemeStore;

namespace {
ResPath     resPath() { return ResPath(std::string(TEST_RES_DIR)); }
std::string fixtureTheme() { return std::string(TEST_DATA_DIR) + "/themeslider.json"; }
std::string fixtureThemeNoSlider() { return std::string(TEST_DATA_DIR) + "/themenoslider.json"; }
} // namespace

// ============================================================================
// Key spellings - the selector both stores agree on
// ============================================================================

TEST(SliderRes, KeysNamePartsAndPointerStates)
{
    // dock- because the slider is dock-only, unlike the scrollbar which the
    // dialog shares; -track/-thumb because they are parts, the way
    // scrollbar-thumb is; and :hover meaning what it means everywhere else.
    // ":active" is deliberately absent - it used to name the filled portion,
    // which is a part wearing a state's clothes, and it is now free for its real
    // meaning if the thumb ever wants drag feedback like the grip has.
    EXPECT_EQ(elementKeyName(ElementKey::DockSliderTrack), "dock-slider-track");
    EXPECT_EQ(elementKeyName(ElementKey::DockSliderThumb), "dock-slider-thumb");
    EXPECT_EQ(elementKeyName(ElementKey::DockSliderThumbHover), "dock-slider-thumb:hover");
}

// ============================================================================
// Layout: the sizes
// ============================================================================

TEST(SliderRes, ShippedLayoutDefinesEveryPart)
{
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    const auto & layout = store.layout();

    EXPECT_GT(layout.sliderTrackH, 0.0F);
    EXPECT_GT(layout.sliderThumbW, 0.0F);
    EXPECT_GT(layout.sliderThumbH, 0.0F);
    EXPECT_GT(layout.sliderThumbHoverW, 0.0F);
    EXPECT_GT(layout.sliderThumbHoverH, 0.0F);
}

TEST(SliderRes, TheHoverThumbIsNeverSmallerThanTheIdleOne)
{
    // The hit area is sized to the hover thumb in BOTH states, so an idle thumb
    // larger than its hovered self would stick out of the region that answers
    // the pointer - the same invariant the scrollbar states for its widening
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    EXPECT_GE(store.layout().sliderThumbHoverW, store.layout().sliderThumbW);
    EXPECT_GE(store.layout().sliderThumbHoverH, store.layout().sliderThumbH);
}

TEST(SliderRes, TheThumbStandsProudOfItsTrack)
{
    // Not a formatting preference: the thumb is what a reader aims at, and a
    // grip no taller than the bar it rides reads as a swollen piece of the bar
    LayoutStore store;
    (void)store.load(resPath().layoutFile());
    EXPECT_GT(store.layout().sliderThumbH, store.layout().sliderTrackH);
}

TEST(SliderRes, MissingLayoutFileLeavesTheSliderAtDefaults)
{
    // Every field falls back rather than throwing, so a host shipping an
    // incomplete res/ degrades instead of failing to start
    LayoutStore store;
    (void)store.load(std::string(TEST_RES_DIR) + "/no-such-layout.json");
    EXPECT_FLOAT_EQ(store.layout().sliderTrackH, 0.0F);
    EXPECT_FLOAT_EQ(store.layout().sliderThumbW, 0.0F);
}

// ============================================================================
// Theme: the colours
// ============================================================================

TEST(SliderRes, EveryBlockLandsInItsOwnPair)
{
    // Six distinct values for six slots, so a block read into the wrong field -
    // or a property read into the wrong half of the pair - names itself in the
    // failure instead of passing because two theme colours happened to match
    ThemeStore store;
    EXPECT_EQ(store.loadTheme(fixtureTheme()), Ui::Res::Type::Changed::Theme);
    const auto & dock = store.theme().dock;

    EXPECT_EQ(dock.sliderTrack.fg.toHex(), "#010203");
    EXPECT_EQ(dock.sliderTrack.bg.toHex(), "#040506");
    EXPECT_EQ(dock.sliderThumb.fg.toHex(), "#070809");
    EXPECT_EQ(dock.sliderThumb.bg.toHex(), "#0a0b0c");
    EXPECT_EQ(dock.sliderThumbHover.fg.toHex(), "#0d0e0f");
    EXPECT_EQ(dock.sliderThumbHover.bg.toHex(), "#101112");
}

TEST(SliderRes, AThemeWithoutSliderBlocksFallsBackToTheErrorColour)
{
    // The shape of the failure a misspelled key produces, and the reason the
    // host-side check looks for THIS rather than for an unset colour: absent
    // block, absent property and mistyped value all resolve to --cl-error alike
    ThemeStore store;
    EXPECT_EQ(store.loadTheme(fixtureThemeNoSlider()), Ui::Res::Type::Changed::Theme);
    const auto & dock = store.theme().dock;

    EXPECT_EQ(dock.sliderTrack.bg.toHex(), "#ff0000");
    EXPECT_EQ(dock.sliderThumb.bg.toHex(), "#ff0000");
    EXPECT_EQ(dock.sliderThumbHover.bg.toHex(), "#ff0000");
}
