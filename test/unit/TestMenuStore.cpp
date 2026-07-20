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
 * @file TestMenuStore.cpp
 * @brief Unit tests for Ui::Res::Store::MenuStore driven through the shared
 *        fixture. Complements the ResManager facade test with the branches
 *        loadAll never hits: the missing-directory no-op and the
 *        reload-identical no-op, plus direct coverage of loadButtons, the
 *        action map, find-by-id/key, and the enable/disable mutators.
 */

#include "menustorefixture.h"
#include "ui/res/respath.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using Ui::Res::ResPath;
using Ui::Res::Type::Changed;

namespace {
ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

struct action_item_t {
    bool        found = false;
    Ui::id_t    id    = Ui::INVALID_ID;
    std::string key;
    std::string label;
    std::string actionKey;
};

action_item_t firstActionItem(const std::vector<Ui::Res::Type::menu_t> & menus)
{
    for (const auto & menu : menus) {
        for (const auto & item : menu.items) {
            if (!item.separator && !item.actionKey.empty()) {
                return { true, item.id, item.key, item.label, item.actionKey };
            }
            for (const auto & sub : item.items) {
                if (!sub.separator && !sub.actionKey.empty()) {
                    return { true, sub.id, sub.key, sub.label, sub.actionKey };
                }
            }
        }
    }
    return {};
}
} // namespace

TEST(MenuStore, MissingDirectoryIsNoOp)
{
    TestSupport::menu_store_fixture_t fixture;
    EXPECT_EQ(fixture.store.loadMenus(std::string(TEST_RES_DIR) + "/no-such-menu-dir"), Changed::None);
    EXPECT_EQ(fixture.store.loadButtons(std::string(TEST_RES_DIR) + "/no-such-button-dir"), Changed::None);
    EXPECT_TRUE(fixture.store.menus().empty());
    EXPECT_TRUE(fixture.store.buttons().empty());
}

TEST(MenuStore, LoadMenusThenReloadIsNoOp)
{
    TestSupport::menu_store_fixture_t fixture;
    // Real layout first so the depth cap / popup metrics are populated.
    fixture.layout.load(resPath().layoutFile());

    EXPECT_EQ(fixture.store.loadMenus(resPath().menuDir()), Changed::Menu);
    EXPECT_FALSE(fixture.store.menus().empty());
    // Re-parsing identical files changes nothing.
    EXPECT_EQ(fixture.store.loadMenus(resPath().menuDir()), Changed::None);
}

TEST(MenuStore, LoadButtons)
{
    TestSupport::menu_store_fixture_t fixture;
    fixture.layout.load(resPath().layoutFile());
    EXPECT_EQ(fixture.store.loadButtons(resPath().buttonDir()), Changed::Button);
    EXPECT_FALSE(fixture.store.buttons().empty());
    // Reload identical files: equal-size vectors force element-wise
    // button_t::operator== and must report no change.
    EXPECT_EQ(fixture.store.loadButtons(resPath().buttonDir()), Changed::None);
}

TEST(MenuStore, ActionMapAndFindRoundTrip)
{
    TestSupport::menu_store_fixture_t fixture;
    fixture.layout.load(resPath().layoutFile());
    fixture.store.loadMenus(resPath().menuDir());
    fixture.store.buildActionMap();

    const action_item_t item = firstActionItem(fixture.store.menus());
    ASSERT_TRUE(item.found);
    EXPECT_EQ(fixture.store.actionKeyFor(item.id), item.actionKey);
    EXPECT_EQ(fixture.store.findMenuItem(item.id).label, item.label);
    EXPECT_EQ(fixture.store.findMenuItemByKey(item.key).label, item.label);
    // Unbound id / unknown key.
    EXPECT_TRUE(fixture.store.actionKeyFor(Ui::INVALID_ID).empty());
    EXPECT_TRUE(fixture.store.findMenuItemByKey("No:Such:Key").label.empty());
}

TEST(MenuStore, EnableDisableMutators)
{
    TestSupport::menu_store_fixture_t fixture;
    fixture.layout.load(resPath().layoutFile());
    fixture.store.loadMenus(resPath().menuDir());

    const action_item_t item = firstActionItem(fixture.store.menus());
    ASSERT_TRUE(item.found);

    fixture.store.setActionEnabled(item.actionKey, false);
    EXPECT_FALSE(fixture.store.findMenuItemByKey(item.key).enabled);
    fixture.store.setActionEnabled(item.actionKey, true);
    EXPECT_TRUE(fixture.store.findMenuItemByKey(item.key).enabled);

    // Per-(action,label): disabling a non-matching label leaves the item enabled.
    fixture.store.setMenuItemEnabled(item.actionKey, "definitely-not-the-label", false);
    EXPECT_TRUE(fixture.store.findMenuItemByKey(item.key).enabled);
    // Disabling the matching label gates it.
    fixture.store.setMenuItemEnabled(item.actionKey, item.label, false);
    EXPECT_FALSE(fixture.store.findMenuItemByKey(item.key).enabled);
}
