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
 * @file TestOpenTabSwitcher.cpp
 * @brief Unit tests for ui/action/opentabswitcher.h, the action on any host with its seams:
 *        "OpenTabSwitcher" opens the Ctrl+Tab list on the tab viewed before the active one or
 *        the least recent, steps it when open already, opens none over a dialog, closes an
 *        open menu, and does nothing for another arg.
 */

#include "ui/action/opentabswitcher.h"
#include "ui/res/type/input.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/tabswitch.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <vector>

namespace {

// The host openTabSwitcher is templated on: its own list, tab bar and input, a dialog and a
// menu it only says are up, and each step and close recorded rather than drawn
struct alignas(128) fake_host_t final {
    Ui::TabSwitch          list;
    Ui::TabBar             bar;
    Ui::Res::Type::input_t inputConfig;
    std::vector<int>       steps; // each cycleTabSwitcher's delta
    int                    menuCloses = 0;
    bool                   isDialogUp = false;
    bool                   isMenuUp   = false;

    [[nodiscard]] Ui::TabSwitch &                tabSwitch() { return list; }
    void                                         cycleTabSwitcher(int delta) { steps.emplace_back(delta); }
    [[nodiscard]] const fake_host_t &            windowManager() const { return *this; }
    [[nodiscard]] const fake_host_t &            dialog() const { return *this; }
    [[nodiscard]] bool                           isOpen() const { return isDialogUp; }
    [[nodiscard]] bool                           isPopupOpen() const { return isMenuUp; }
    [[nodiscard]] const fake_host_t &            resManager() const { return *this; }
    [[nodiscard]] const Ui::TabBar &             tabBar() const { return bar; }
    [[nodiscard]] const Ui::Res::Type::input_t & input() const { return inputConfig; }

    void closePopupMenu()
    {
        isMenuUp = false;
        ++menuCloses;
    }
};

// Tabs 1, 2, 3 on the strip; 3 the active one, 1 viewed before it, 2 the least recent
void project(fake_host_t & host)
{
    host.bar.setTabs({ Ui::tab_t { .id = 1 }, Ui::tab_t { .id = 2 }, Ui::tab_t { .id = 3, .isActive = true } },
                     { 3, 1, 2 });
}

} // namespace

TEST(OpenTabSwitcherTest, NextOpensOnTheTabViewedBefore)
{
    fake_host_t host;
    project(host);
    Ui::Action::openTabSwitcher(host, "next");
    EXPECT_TRUE(host.list.isOpen());
    EXPECT_EQ(host.list.selectedId(), Ui::id_t { 1 });
    EXPECT_FALSE(host.list.isShown()) << "up only once its delay has run out";
}

TEST(OpenTabSwitcherTest, PriorOpensOnTheLeastRecent)
{
    fake_host_t host;
    project(host);
    Ui::Action::openTabSwitcher(host, "prior");
    EXPECT_EQ(host.list.selectedId(), Ui::id_t { 2 });
}

// The key that opened it, again with the list open: a step, never a list opened again
TEST(OpenTabSwitcherTest, OpenAlreadyItSteps)
{
    fake_host_t host;
    project(host);
    Ui::Action::openTabSwitcher(host, "next");
    Ui::Action::openTabSwitcher(host, "prior");
    EXPECT_EQ(host.steps, (std::vector<int> { -1 }));
    EXPECT_EQ(host.list.selectedId(), Ui::id_t { 1 });
}

TEST(OpenTabSwitcherTest, NoneOverADialog)
{
    fake_host_t host;
    project(host);
    host.isDialogUp = true;
    Ui::Action::openTabSwitcher(host, "next");
    EXPECT_FALSE(host.list.isOpen());
}

TEST(OpenTabSwitcherTest, AnOpenMenuCloses)
{
    fake_host_t host;
    project(host);
    host.isMenuUp = true;
    Ui::Action::openTabSwitcher(host, "next");
    EXPECT_EQ(host.menuCloses, 1);
    EXPECT_TRUE(host.list.isOpen());
}

TEST(OpenTabSwitcherTest, AnyOtherArgDoesNothing)
{
    fake_host_t host;
    project(host);
    Ui::Action::openTabSwitcher(host, "sideways");
    EXPECT_FALSE(host.list.isOpen());
    EXPECT_TRUE(host.steps.empty());
}
