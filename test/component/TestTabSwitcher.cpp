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
 * @file TestTabSwitcher.cpp
 * @brief The Ctrl+Tab list driven through the shell's own loop: Ctrl going up, put in the
 *        OS's queue as the OS puts it, switches to the tab the list has highlighted - with
 *        no list after a quick tap, and taking it down once it is up. Posted rather than
 *        handed to the handler, so a release the window never asked the OS for (X11 sends
 *        one only to a window that selected KeyReleaseMask) never comes here either
 *
 * Not on native Wayland, where no client puts key events in its own queue
 */

#if defined(_WIN32) || defined(__APPLE__) || defined(HAVE_X11)

#include "common/time.h"
#include "menubarloop.h"
#include "nativeinput.h"
#include "shelltest.h"
#include "ui/config.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/tab.h"
#include "ui/type.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <vector>

namespace {

class TabSwitcherTest : public PureGlUi::ShellTest {
protected:
    std::vector<Ui::id_t> m_activated; // the tabs a switch activated, in order

    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        m_shell.setOnTabActivated([this](Ui::id_t id) { m_activated.emplace_back(id); });
        // Tabs 1, 2, 3 on the strip; 3 the active one, 1 viewed before it, 2 the least recent
        m_shell.resManager().tabBar().setTabs(
        { Ui::tab_t { .id = 1 }, Ui::tab_t { .id = 2 }, Ui::tab_t { .id = 3, .isActive = true } },
        { 3, 1, 2 });
    }
};

} // namespace

// A quick tap: Ctrl goes up before the list is due, and the tab viewed before the active
// one is activated with no list ever up
TEST_F(TabSwitcherTest, ACtrlTapSwitchesToTheTabViewedBefore)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 1 })) << m_read;
    EXPECT_FALSE(m_shell.windowManager().hasTabSwitcher()) << m_read;
}

// Ctrl held past the delay: the list goes up, and Ctrl going up then switches to the tab it
// has highlighted and takes it down
TEST_F(TabSwitcherTest, CtrlGoingUpWithTheListUpSwitchesAndTakesItDown)
{
    bool isListUp = false;
    bool isPosted = false;
    PureGlUi::runPasses(
    [this, &isListUp, &isPosted](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        if (!isPosted && m_shell.windowManager().hasTabSwitcher()) {
            isListUp = true;
            isPosted = true;
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    ASSERT_TRUE(isListUp) << "the list never went up; " << m_read;
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 1 })) << m_read;
    EXPECT_FALSE(m_shell.windowManager().hasTabSwitcher()) << m_read;
}

// Not on macOS, where a move posted to the list's window comes back in the main window's px
#if defined(_WIN32) || defined(HAVE_X11)

// The pointer over the list hovers a row, which every move hands the highlight to; moved off
// onto the main window it hovers none, or a row Ctrl+Tab highlights since is taken back by the
// next move, and Ctrl going up switches to the row last hovered
TEST_F(TabSwitcherTest, APointerMovedOffTheListHoversNoRow)
{
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    int                         step          = 0;
    std::optional<std::size_t>  overRow; // hovered with the pointer over the list
    std::optional<std::size_t>  offRow;  // hovered with it moved off
    PureGlUi::runPasses(
    [this, &windowManager, &step, &overRow, &offRow](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        const Ui::Window::Popup::PopupWindow * list = windowManager.tabSwitcherWindow();
        if (list == nullptr) {
            return;
        }
        const Ui::Res::Type::bound_t bound = list->bound();
        if (step == 0) {
            // Its middle: on a window of its own at its own px, drawn into the main window at the main window's
            if (Ui::g_config.isCompositing) {
                PureGlUi::postPointerMove(mainWindow(),
                                          Ui::roundToInt(bound.x + (bound.w / 2.0F)),
                                          Ui::roundToInt(bound.y + (bound.h / 2.0F)));
            } else {
                PureGlUi::postPointerMove(*list, Ui::roundToInt(bound.w / 2.0F), Ui::roundToInt(bound.h / 2.0F));
            }
        } else if (step == 1) {
            overRow = windowManager.tabSwitcherHoveredRow();
            // Left of it, on the main window
            PureGlUi::postPointerMove(mainWindow(),
                                      Ui::roundToInt(bound.x / 2.0F),
                                      Ui::roundToInt(bound.y + (bound.h / 2.0F)));
        } else if (step == 2) {
            offRow = windowManager.tabSwitcherHoveredRow();
        }
        ++step;
    },
    m_shell);
    ASSERT_GE(step, 3) << "the list went up too late to move over it; " << m_read;
    ASSERT_TRUE(overRow.has_value()) << "the pointer over the list hovered no row; " << m_read;
    EXPECT_FALSE(offRow.has_value()) << "row " << offRow.value_or(0) << " still hovered off the list; " << m_read;
}

#endif

#endif
