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
 * @file TestCycleTab.cpp
 * @brief Unit tests for ui/action/cycletab.h: "CycleTab" activates the tab after the
 *        active one, the first after the last, and nothing with one tab, none, or
 *        none active.
 */

#include "ui/action/cycletab.h"
#include "ui/tab.h"
#include "ui/tabbar.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <vector>

namespace {

// The host cycleTab is templated on, recording the tab it activates rather than
// activating it; its own resManager, since the tab bar is all cycleTab reads of one
struct alignas(64) fake_host_t final {
    Ui::TabBar            bar;
    std::vector<Ui::id_t> activated;

    [[nodiscard]] const fake_host_t & resManager() const { return *this; }
    [[nodiscard]] const Ui::TabBar &  tabBar() const { return bar; }
    void                              setActiveTab(Ui::id_t tabId) { activated.emplace_back(tabId); }
};

// Tabs 1..count in order, tab `active` the active one; 0 for none
std::vector<Ui::tab_t> tabsOf(Ui::id_t count, Ui::id_t active)
{
    std::vector<Ui::tab_t> tabs;
    for (Ui::id_t id = 1; id <= count; ++id) {
        tabs.emplace_back(Ui::tab_t { id, "tab", id == active });
    }
    return tabs;
}

} // namespace

TEST(CycleTabTest, NoTabsActivatesNothing)
{
    fake_host_t host;
    Ui::Action::cycleTab(host, "");
    EXPECT_TRUE(host.activated.empty());
}

TEST(CycleTabTest, OneTabActivatesNothing)
{
    fake_host_t host;
    host.bar.setTabs(tabsOf(1, 1));
    Ui::Action::cycleTab(host, "");
    EXPECT_TRUE(host.activated.empty());
}

TEST(CycleTabTest, NoActiveTabActivatesNothing)
{
    fake_host_t host;
    host.bar.setTabs(tabsOf(3, 0));
    Ui::Action::cycleTab(host, "");
    EXPECT_TRUE(host.activated.empty());
}

TEST(CycleTabTest, TheTabAfterTheActiveOne)
{
    fake_host_t host;
    host.bar.setTabs(tabsOf(3, 2));
    Ui::Action::cycleTab(host, "");
    EXPECT_EQ(host.activated, (std::vector<Ui::id_t> { 3 }));
}

TEST(CycleTabTest, TheFirstAfterTheLast)
{
    fake_host_t host;
    host.bar.setTabs(tabsOf(3, 3));
    Ui::Action::cycleTab(host, "");
    EXPECT_EQ(host.activated, (std::vector<Ui::id_t> { 1 }));
}

// The host re-projects the tab it activated, as App does, before the next press
TEST(CycleTabTest, TwoTabsCycledTwiceComeBack)
{
    fake_host_t host;
    host.bar.setTabs(tabsOf(2, 1));
    Ui::Action::cycleTab(host, "");
    host.bar.setTabs(tabsOf(2, 2));
    Ui::Action::cycleTab(host, "");
    EXPECT_EQ(host.activated, (std::vector<Ui::id_t> { 2, 1 }));
}
