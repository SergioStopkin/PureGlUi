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
 *        no list after a quick tap, and taking it down once it is up; a row's close button
 *        closing its tab with the list kept up. Posted rather than
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
#include "ui/render/rowcloseboundof.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/tab.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"
#include "ui/window/keymodifier.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <gtest/gtest.h>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {

class TabSwitcherTest : public PureGlUi::ShellTest {
protected:
    std::vector<Ui::id_t> m_activated; // the tabs a switch activated, in order
    std::vector<Ui::id_t> m_closed;    // the tabs a close button closed, in order

    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
#ifdef _WIN32
        // The OS's pointer off the list, at the status bar's left end its margin keeps clear: a move
        // Windows sends for it resting over a row would hover that row and take the highlight
        PureGlUi::placePointer(mainWindow(), 0, Ui::roundToInt(mainWindow().bound().h) - 1);
#endif
        m_shell.setOnTabActivated([this](Ui::id_t id) { m_activated.emplace_back(id); });
        // Tabs 1, 2, 3 on the strip; 3 the active one, 1 viewed before it, 2 the least recent
        m_shell.resManager().tabBar().setTabs(
        { Ui::tab_t { .id = 1 }, Ui::tab_t { .id = 2 }, Ui::tab_t { .id = 3, .isActive = true } },
        { 3, 1, 2 });
    }

    // `name` going down with `modifiers` held, handed on as the loop hands a key: to the shell
    // through the window manager
    void pressKey(std::string_view name, Ui::Window::key_modifiers_t modifiers)
    {
        Ui::Window::Event key;
        key.type          = Ui::Window::EventType::KeyPress;
        key.key.modifiers = modifiers;
        std::ranges::copy(name, key.key.name.begin());
        m_shell.windowManager().dispatchEvent(key);
    }

    void pressWithControl(std::string_view name)
    {
        Ui::Window::key_modifiers_t control;
        control |= Ui::Window::KeyModifier::Control;
        pressKey(name, control);
    }

    // Pointer input at (`x`, `y`) of the open list, its own px: on the list's own window, or
    // drawn into the main window at the main window's
    template <typename Post>
    void postAt(int x, int y, Post post)
    {
        const Ui::Window::Popup::PopupWindow & list  = *m_shell.windowManager().tabSwitcherWindow();
        const Ui::Res::Type::bound_t           bound = list.bound();
        if (Ui::g_config.isCompositing) {
            post(mainWindow(), Ui::roundToInt(bound.x) + x, Ui::roundToInt(bound.y) + y);
        } else {
            post(list, x, y);
        }
    }

    // Pointer input at the middle of the open list's `row`
    template <typename Post>
    void postAtRow(int row, Post post)
    {
        const Ui::Res::Type::bound_t bound = m_shell.windowManager().tabSwitcherWindow()->bound();
        const Ui::fpx_t              rowH  = m_shell.resManager().layout().tabSwitcherItem.height;
        postAt(Ui::roundToInt(bound.w / 2.0F), Ui::roundToInt(Ui::toPhys((row + 0.5F) * rowH)), post);
    }

    // Pointer input at the middle of the open list's `row`'s close button, the list not
    // scrolling - its rows its whole width
    template <typename Post>
    void postAtClose(int row, Post post)
    {
        const Ui::Res::Type::bound_t bound = m_shell.windowManager().tabSwitcherWindow()->bound();
        const Ui::fpx_t              rowH  = m_shell.resManager().layout().tabSwitcherItem.height;
        const Ui::Res::Type::bound_t close = Ui::Render::rowCloseBoundOf(
        m_shell.resManager().layout().tabSwitcherClose,
        { .x = 0.0F, .y = static_cast<Ui::fpx_t>(row) * rowH, .w = Ui::toCss(bound.w), .h = rowH });
        postAt(Ui::roundToInt(Ui::toPhys(close.x + (close.w / 2.0F))),
               Ui::roundToInt(Ui::toPhys(close.y + (close.h / 2.0F))),
               post);
    }

    // A close taking its tab off as a host does, the one it closed noted
    void closeAsAHost()
    {
        m_shell.setOnTabClosed([this](Ui::id_t closedId) {
            m_closed.emplace_back(closedId);
            projectTabs(closedId);
        });
    }

    // Tabs 3, 1, 2 by when last active as SetUp has them, but `closedId`
    void projectTabs(Ui::id_t closedId)
    {
        std::vector<Ui::tab_t> tabs;
        std::vector<Ui::id_t>  recentIds;
        for (const Ui::id_t id : { Ui::id_t { 3 }, Ui::id_t { 1 }, Ui::id_t { 2 } }) {
            if (id != closedId) {
                tabs.emplace_back(Ui::tab_t { .id = id, .isActive = id == 3 });
                recentIds.emplace_back(id);
            }
        }
        m_shell.resManager().tabBar().setTabs(std::move(tabs), std::move(recentIds));
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

// The action again with the list open steps it, as the key bound to it does while Ctrl is
// held: Ctrl going up switches to the tab two rows on, not the one the list opened on
TEST_F(TabSwitcherTest, OpeningItAgainStepsTheOpenList)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 2 })) << m_read;
}

// The list steps on the key res binds to OpenTabSwitcher, and the framework's own shortcut.json
// binds it to none: Ctrl+Tab, bound to nothing, leaves the open list where it is
TEST_F(TabSwitcherTest, AKeyBoundToNothingLeavesTheOpenListAlone)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            pressWithControl("Tab");
            // Ignored, not taken for Ctrl going up: the list still open, nothing switched yet
            EXPECT_TRUE(m_shell.tabSwitch().isOpen()) << m_read;
            EXPECT_TRUE(m_activated.empty()) << m_read;
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 1 })) << m_read;
}

// Ctrl+Down steps the open list a row: Ctrl going up switches to the tab after the one it opened on
TEST_F(TabSwitcherTest, CtrlDownStepsTheList)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            pressWithControl("Down");
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 2 })) << m_read;
}

// Ctrl+Escape takes the list down unswitched, and Ctrl going up after it switches to nothing
TEST_F(TabSwitcherTest, CtrlEscapeTakesItDownUnswitched)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            pressWithControl("Escape");
            EXPECT_FALSE(m_shell.tabSwitch().isOpen()) << m_read;
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_TRUE(m_activated.empty()) << m_read;
}

// A shortcut behind the open list never runs: Ctrl+Next, the framework's CycleTab, leaves it
// open with nothing switched, and Ctrl going up then takes what it has highlighted
TEST_F(TabSwitcherTest, AShortcutBehindTheOpenListNeverRuns)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            pressWithControl("Next");
            EXPECT_TRUE(m_shell.tabSwitch().isOpen()) << m_read;
            EXPECT_TRUE(m_activated.empty()) << m_read;
            PureGlUi::postControlRelease(mainWindow());
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 1 })) << m_read;
}

// A key with Ctrl already up - its release gone unseen - switches first, as Ctrl going up would
TEST_F(TabSwitcherTest, AKeyWithCtrlUpSwitchesFirst)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            pressKey("a", {});
        }
    },
    m_shell);
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 1 })) << m_read;
    EXPECT_FALSE(m_shell.tabSwitch().isOpen()) << m_read;
}

// Not on macOS, where a move posted to the list's window comes back in the main window's px
#if defined(_WIN32) || defined(HAVE_X11)

// The pointer over a row highlights it, and Ctrl going up switches to its tab: the last row, the
// least recent tab, not the one the list opened on
TEST_F(TabSwitcherTest, AHoveredRowIsWhatCtrlGoingUpTakes)
{
    int         step     = 0;
    std::size_t selected = 0;
    PureGlUi::runPasses(
    [this, &step, &selected](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        if (m_shell.windowManager().tabSwitcherWindow() == nullptr) {
            return;
        }
        if (step == 0) {
            postAtRow(2, PureGlUi::postPointerMove);
        } else if (step == 1) {
            selected = m_shell.tabSwitch().selectedRow();
            PureGlUi::postControlRelease(mainWindow());
        }
        ++step;
    },
    m_shell);
    ASSERT_GE(step, 2) << "the list went up too late to hover it; " << m_read;
    EXPECT_EQ(selected, 2U) << m_read;
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 2 })) << m_read;
}

// A click on a row is one on its tab: it switches there and the list goes down
TEST_F(TabSwitcherTest, AClickOnARowSwitchesToItsTab)
{
    bool isClicked = false;
    PureGlUi::runPasses(
    [this, &isClicked](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        if (isClicked || m_shell.windowManager().tabSwitcherWindow() == nullptr) {
            return;
        }
        postAtRow(2, PureGlUi::postPointerPress);
        postAtRow(2, PureGlUi::postPointerRelease);
        isClicked = true;
    },
    m_shell);
    ASSERT_TRUE(isClicked) << "the list never went up; " << m_read;
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { 2 })) << m_read;
    EXPECT_FALSE(m_shell.windowManager().hasTabSwitcher()) << m_read;
}

// A click on a row's close button closes its tab, switching to none: the list stays up, Ctrl
// still held, a row per tab left - the closed one's row, the last, gone and the highlight on the
// row it stood on
TEST_F(TabSwitcherTest, AClickOnARowsCloseButtonClosesItsTabTheListStayingUp)
{
    closeAsAHost();
    bool isClicked = false;
    PureGlUi::runPasses(
    [this, &isClicked](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        if (isClicked || m_shell.windowManager().tabSwitcherWindow() == nullptr) {
            return;
        }
        postAtClose(2, PureGlUi::postPointerPress);
        postAtClose(2, PureGlUi::postPointerRelease);
        isClicked = true;
    },
    m_shell);
    ASSERT_TRUE(isClicked) << "the list never went up; " << m_read;
    EXPECT_EQ(m_closed, (std::vector<Ui::id_t> { 2 })) << m_read;
    EXPECT_TRUE(m_activated.empty()) << m_read;
    EXPECT_TRUE(m_shell.windowManager().hasTabSwitcher()) << m_read;
    EXPECT_EQ(m_shell.tabSwitch().ids(), (std::vector<Ui::id_t> { 3, 1 })) << m_read;
    EXPECT_EQ(m_shell.tabSwitch().selectedId(), 1U) << m_read;
}

// A close button pressed and let go of off it - on its row's label - closes nothing and is no
// click on the row: no switch, the list still up
TEST_F(TabSwitcherTest, ACloseButtonLetGoOfOffItClosesNothing)
{
    closeAsAHost();
    bool isClicked = false;
    PureGlUi::runPasses(
    [this, &isClicked](int pass) {
        if (pass == 0) {
            m_shell.actions().dispatch("OpenTabSwitcher", "next");
            Common::Time::sleepFor(std::chrono::milliseconds(m_shell.resManager().input().tabSwitcherDelayMs + 50));
            return;
        }
        if (isClicked || m_shell.windowManager().tabSwitcherWindow() == nullptr) {
            return;
        }
        postAtClose(2, PureGlUi::postPointerPress);
        postAtRow(2, PureGlUi::postPointerRelease);
        isClicked = true;
    },
    m_shell);
    ASSERT_TRUE(isClicked) << "the list never went up; " << m_read;
    EXPECT_TRUE(m_closed.empty()) << m_read;
    EXPECT_TRUE(m_activated.empty()) << m_read;
    EXPECT_TRUE(m_shell.windowManager().hasTabSwitcher()) << m_read;
}
// The pointer over the list hovers a row; moved off onto the main window it hovers none, or a row
// Ctrl+Tab highlights since would be taken back by the next move
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
