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
 * @file TestShortcutStore.cpp
 * @brief Unit tests for Ui::Res::Store::ShortcutStore. Loads res/shortcut.json
 *        (TEST_RES_DIR), verifies the normalized-combo -> actionKey map, the
 *        Changed-bit semantics (reload of identical data is a no-op), and the
 *        missing-file no-op.
 */

#include "ui/res/respath.h"
#include "ui/res/store/shortcutstore.h"
#include "ui/res/type/changed.h"

#include <algorithm>
#include <gtest/gtest.h>
#include <string>

using Ui::Res::ResPath;
using Ui::Res::Store::ShortcutStore;
using Ui::Res::Type::Changed;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

bool mapsToAction(const ShortcutStore & store, const std::string & action)
{
    return std::any_of(store.shortcuts().begin(), store.shortcuts().end(), [&action](const auto & pair) {
        return pair.second == action;
    });
}
} // namespace

TEST(ShortcutStore, LoadShippedShortcuts)
{
    ShortcutStore store;
    EXPECT_EQ(store.load(resPath().shortcutFile()), Changed::Shortcut);
    EXPECT_FALSE(store.shortcuts().empty());
    // The shipped file binds these actions (combo spelling is normalized by
    // Util::strKey, so we assert on the action value, not the combo key).
    EXPECT_TRUE(mapsToAction(store, "ExitApp"));
    EXPECT_TRUE(mapsToAction(store, "Reload"));
    EXPECT_TRUE(mapsToAction(store, "OpenFile"));
}

TEST(ShortcutStore, ReloadIdenticalIsNoOp)
{
    ShortcutStore store;
    (void)store.load(resPath().shortcutFile());
    EXPECT_EQ(store.load(resPath().shortcutFile()), Changed::None);
}

TEST(ShortcutStore, MissingFileIsNoOp)
{
    ShortcutStore store;
    EXPECT_EQ(store.load(std::string(TEST_RES_DIR) + "/no-such-shortcut.json"), Changed::None);
    EXPECT_TRUE(store.shortcuts().empty());
}
