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
 * @file TestResManager.cpp
 * @brief Unit tests for the Ui::Res::ResManager facade over the shipped res/
 *        (TEST_RES_DIR). loadAll() composes every sub-store, so this exercises
 *        the MenuStore load/parse/action-map path end-to-end with real
 *        collaborators, plus the facade queries: find-by-id / find-by-key, the
 *        id->actionKey map, the radio-highlight provider, the enable/disable
 *        mutators, the theme name/mode switch, and the persisted-setting registry.
 */

#include "ui/res/key/section.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

using Ui::Res::ResManager;
using Ui::Res::Key::SectionKey;
using Ui::Res::Type::Changed;

namespace {
// First actionable (non-separator, has actionKey) menu item at item or submenu
// depth. Top-level menus are skipped: the enable/disable walker operates on a
// menu's items, not the menu node itself.
struct action_item_t {
    bool        found = false;
    Ui::id_t    id    = Ui::INVALID_ID;
    std::string key;
    std::string actionKey;
};

action_item_t firstActionItem(const std::vector<Ui::Res::Type::menu_t> & menus)
{
    for (const auto & menu : menus) {
        for (const auto & item : menu.items) {
            if (!item.separator && !item.actionKey.empty()) {
                return { true, item.id, item.key, item.actionKey };
            }
            for (const auto & sub : item.items) {
                if (!sub.separator && !sub.actionKey.empty()) {
                    return { true, sub.id, sub.key, sub.actionKey };
                }
            }
        }
    }
    return {};
}
} // namespace

TEST(ResManager, LoadAllPopulatesEverything)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    EXPECT_NE(rm.changed(), Changed::None);
    EXPECT_FALSE(rm.title().empty());
    EXPECT_FALSE(rm.menus().empty());
    EXPECT_FALSE(rm.shortcuts().empty());
    EXPECT_GT(rm.popup().itemHeight, 0.0F);
    EXPECT_GT(rm.layout().topMenu.height, 0.0F);
}

TEST(ResManager, FindMenuItemByIdAndKey)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();
    ASSERT_FALSE(rm.menus().empty());

    const Ui::Res::Type::menu_t first = rm.menus().front();
    EXPECT_EQ(rm.findMenuItem(first.id).label, first.label);
    EXPECT_EQ(rm.findMenuItemByKey(first.key).label, first.label);

    // Unknown id / key resolve to a default-constructed (empty-label) node.
    EXPECT_TRUE(rm.findMenuItem(Ui::INVALID_ID).label.empty());
    EXPECT_TRUE(rm.findMenuItemByKey("No:Such:Key").label.empty());
}

TEST(ResManager, ActionKeyForResolvesLoadedItems)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    const action_item_t item = firstActionItem(rm.menus());
    ASSERT_TRUE(item.found) << "expected at least one menu item with an action";
    EXPECT_EQ(rm.actionKeyFor(item.id), item.actionKey);
    // An id with no bound action yields the empty string.
    EXPECT_TRUE(rm.actionKeyFor(Ui::INVALID_ID).empty());
}

TEST(ResManager, ThemeNameAndModeSwitch)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    EXPECT_EQ(rm.themeName(), "default");
    EXPECT_TRUE(rm.isThemeDark());

    rm.switchThemeMode();
    EXPECT_FALSE(rm.isThemeDark());

    rm.setThemeName("cobalt");
    EXPECT_EQ(rm.themeName(), "cobalt");
}

TEST(ResManager, IsActiveItemFollowsProvider)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    rm.setActionValueProvider("SwitchThemeMode", [] { return std::string("Dark"); });

    Ui::Res::Type::menu_t item;
    item.actionKey = "SwitchThemeMode";
    item.label     = "Dark";
    EXPECT_TRUE(rm.isActiveItem(item)); // label == provider's current value

    item.label = "Light";
    EXPECT_FALSE(rm.isActiveItem(item));

    // An empty label is never the active choice.
    Ui::Res::Type::menu_t blank;
    blank.actionKey = "SwitchThemeMode";
    EXPECT_FALSE(rm.isActiveItem(blank));
}

TEST(ResManager, SetActionEnabledTogglesLoadedItem)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    const action_item_t item = firstActionItem(rm.menus());
    ASSERT_TRUE(item.found);

    rm.setActionEnabled(item.actionKey, false);
    EXPECT_FALSE(rm.findMenuItemByKey(item.key).enabled);

    rm.setActionEnabled(item.actionKey, true);
    EXPECT_TRUE(rm.findMenuItemByKey(item.key).enabled);
}

TEST(ResManager, RegisterPersistedAppearsInSession)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    rm.registerPersisted(
    SectionKey::Paths,
    "probeKey",
    [] { return std::string("probeVal"); },
    [](const std::string &) {});

    const std::string session = rm.serializeSession();
    EXPECT_NE(session.find("probeKey"), std::string::npos);
    EXPECT_NE(session.find("probeVal"), std::string::npos);
}

TEST(ResManager, SwitchThemeProviderHighlightsCurrentTheme)
{
    ResManager rm { TEST_RES_DIR };
    rm.loadAll();

    // loadAll registers the built-in "SwitchTheme" value provider (-> the current
    // theme name); the theme-submenu radio highlight consults it via isActiveItem.
    Ui::Res::Type::menu_t themeItem;
    themeItem.actionKey = "SwitchTheme";
    themeItem.label     = rm.themeName(); // matches the provider's value ("default")
    EXPECT_TRUE(rm.isActiveItem(themeItem));

    themeItem.label = "not-the-current-theme";
    EXPECT_FALSE(rm.isActiveItem(themeItem));
}
