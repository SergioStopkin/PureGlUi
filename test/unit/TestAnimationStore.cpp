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
 * @file TestAnimationStore.cpp
 * @brief Unit tests for Ui::Res::Store::AnimationStore: the shipped res/animation.json
 *        (TEST_RES_DIR) gives each element a renderer animates a transition, a
 *        block it does not name moves nothing, and no file - or one taken away before a
 *        reload - leaves everything jumping; workspace-tab's swap-at read, half when the
 *        file names none, and held within 0..1
 */

#include "ui/res/key/element.h"
#include "ui/res/respath.h"
#include "ui/res/store/animationstore.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <string>

using Ui::Res::Key::ElementKey;
using Ui::Res::Key::elementKeyName;
using Ui::Res::Store::AnimationStore;

namespace {

Ui::Res::ResPath resPath() { return Ui::Res::ResPath(std::string(TEST_RES_DIR)); }

const std::string MISSING = std::string(TEST_RES_DIR) + "/does-not-exist.json";

constexpr Ui::fpx_t HALF = 0.5F;

} // namespace

// What a renderer reads a transition for: read wrong, it jumps - and the dialog's key press
// shows each of its steps for no time at all
TEST(AnimationStore, ShippedFileAnimatesEveryElementARendererDoes)
{
    AnimationStore store;
    store.load(resPath().animationFile());
    for (const ElementKey key : { ElementKey::WorkspaceTab,
                                  ElementKey::WorkspaceTabClose,
                                  ElementKey::ToolbarButton,
                                  ElementKey::TopMenuButtonLabel,
                                  ElementKey::TopMenuItem,
                                  ElementKey::DialogButton,
                                  ElementKey::DialogClose }) {
        EXPECT_FALSE(store.transitionsFor(elementKeyName(key)).empty()) << elementKeyName(key);
    }
}

TEST(AnimationStore, ABlockNotNamedMovesNothing)
{
    AnimationStore store;
    store.load(resPath().animationFile());
    EXPECT_TRUE(store.transitionsFor(elementKeyName(ElementKey::StatusBar)).empty());
}

TEST(AnimationStore, NoFileJumps)
{
    AnimationStore store;
    store.load(MISSING);
    EXPECT_TRUE(store.transitionsFor(elementKeyName(ElementKey::WorkspaceTab)).empty());
}

TEST(AnimationStore, ReloadDropsWhatTheFileNoLongerHas)
{
    AnimationStore store;
    store.load(resPath().animationFile());
    store.load(MISSING);
    EXPECT_TRUE(store.transitionsFor(elementKeyName(ElementKey::WorkspaceTab)).empty());
}

TEST(AnimationStore, TabSwapsHalfOverANeighbourByTheShippedFile)
{
    AnimationStore store;
    store.load(resPath().animationFile());
    EXPECT_FLOAT_EQ(store.tabSwapAt(), HALF);
}

// No file is no motion, but a drag still reorders - at half
TEST(AnimationStore, NoFileSwapsAtHalf)
{
    AnimationStore store;
    store.load(MISSING);
    EXPECT_FLOAT_EQ(store.tabSwapAt(), HALF);
}

TEST(AnimationStore, ASwapPastAWholeTabIsAWholeTab)
{
    AnimationStore store;
    store.load(std::string(TEST_DATA_DIR) + "/animationswapbeyond.json");
    EXPECT_FLOAT_EQ(store.tabSwapAt(), 1.0F);
}
