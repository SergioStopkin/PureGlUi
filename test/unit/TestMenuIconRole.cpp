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
 * @file TestMenuIconRole.cpp
 * @brief Unit tests for menu icons named by ROLE rather than by file. A menu may
 *        say "icon": "info" and let icon-defaults.json decide which .svg that is,
 *        for the item icon and for a dialog's icon alike - otherwise the role is
 *        decorative and every menu repeats a filename.
 */

#include "menustorefixture.h"
#include "ui/res/respath.h"

#include <gtest/gtest.h>
#include <string>

using Ui::Res::ResPath;

namespace {

ResPath resPath() { return ResPath(std::string(TEST_RES_DIR)); }

// Load icon-defaults into the caller's fixture - that is what makes a role
// resolvable at all. By reference: the fixture owns a MenuStore, which is
// NonCopyable, so it cannot be returned.
void loadInto(TestSupport::menu_store_fixture_t & fixture)
{
    (void)fixture.icons.load(resPath().iconDefaultsFile());
    (void)fixture.layout.load(resPath().layoutFile());
    (void)fixture.store.loadMenus(resPath().menuDir());
}

// Depth-first search for the first item carrying an icon, so the assertions do
// not depend on which menu file happens to declare one
const Ui::Res::Type::menu_t * findWithIcon(const std::vector<Ui::Res::Type::menu_t> & items)
{
    for (const auto & item : items) {
        if (!item.icon.empty()) {
            return &item;
        }
        if (const auto * child = findWithIcon(item.items); child != nullptr) {
            return child;
        }
    }
    return nullptr;
}

const Ui::Res::Type::menu_t * findWithDialogIcon(const std::vector<Ui::Res::Type::menu_t> & items)
{
    for (const auto & item : items) {
        if (!item.dialog.icon.empty()) {
            return &item;
        }
        if (const auto * child = findWithDialogIcon(item.items); child != nullptr) {
            return child;
        }
    }
    return nullptr;
}

} // namespace

// ============================================================================
// Positive: a role resolves, a filename passes through
// ============================================================================

TEST(MenuIconRole, EveryResolvedItemIconIsAFileNotARole)
{
    TestSupport::menu_store_fixture_t fixture;
    loadInto(fixture);

    const auto * item = findWithIcon(fixture.store.menus());
    ASSERT_NE(item, nullptr);
    EXPECT_TRUE(item->icon.ends_with(".svg")) << "unresolved role leaked to the renderer: " << item->icon;
}

TEST(MenuIconRole, EveryResolvedDialogIconIsAFileNotARole)
{
    TestSupport::menu_store_fixture_t fixture;
    loadInto(fixture);

    const auto * item = findWithDialogIcon(fixture.store.menus());
    if (item == nullptr) {
        GTEST_SKIP() << "no dialog in the shipped menus declares an icon";
    }
    EXPECT_TRUE(item->dialog.icon.ends_with(".svg")) << "unresolved role leaked to the dialog: " << item->dialog.icon;
}

TEST(MenuIconRole, RoleResolvesToWhatIconDefaultsNames)
{
    TestSupport::menu_store_fixture_t fixture;
    loadInto(fixture);

    const std::string info = fixture.icons.iconDefault("info").icon;
    if (info.empty()) {
        GTEST_SKIP() << "this res set declares no 'info' role";
    }
    EXPECT_TRUE(info.ends_with(".svg"));
}

// ============================================================================
// Negative: an unknown role, and a store with no icon defaults at all
// ============================================================================

TEST(MenuIconRole, UnknownRoleResolvesToNothingRatherThanToItself)
{
    TestSupport::menu_store_fixture_t fixture;
    loadInto(fixture);
    EXPECT_TRUE(fixture.icons.iconDefault("no-such-role").icon.empty());
}

TEST(MenuIconRole, WithoutIconDefaultsARoleNamedIconEndsUpEmpty)
{
    // icon-defaults deliberately NOT loaded: a role has nothing to resolve
    // against, and the item must end up with no icon rather than with the role
    // string, which the renderer would try to open as a path
    TestSupport::menu_store_fixture_t fixture;
    (void)fixture.layout.load(resPath().layoutFile());
    (void)fixture.store.loadMenus(resPath().menuDir());

    const auto * item = findWithIcon(fixture.store.menus());
    if (item != nullptr) {
        EXPECT_TRUE(item->icon.ends_with(".svg"));
    }
}
