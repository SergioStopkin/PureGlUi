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
 * @file TestMenuBarWindowChange.cpp
 * @brief A press on a title opens its list whatever its window was just doing, an open list
 * closes as its app loses the focus, and on macOS Quit ends the app however its window stands
 *
 * A press goes in the OS's queue at a title worked out from the window as it stands
 * (nativeinput.h), so a window is settled first: macOS places a posted press against the
 * frame the window had before its latest change until AppKit has run its loop - a real
 * click comes from the window server, against the frame it shows - so settle() waits for
 * the window server to have it. Each case here changes the window round a press one way,
 * as the OS or the user would (nativegeometry.h) - made after a shell that never ran its
 * loop, moved or resized in the batch its press comes in, or after the press was queued.
 * A shell before can leave its close request or its size for the next one to take as its
 * own, and the OS can give a window another size than the shell asked for. Every failure
 * says what the loop read
 *
 * Not on native Wayland, where no client puts pointer events in its own queue
 */

#if defined(_WIN32) || defined(__APPLE__) || (defined(HAVE_X11) && !defined(HAVE_WAYLAND))

#include "menubarloop.h"
#include "nativefocus.h"
#include "nativegeometry.h"
#include "nativeinput.h"
#include "testconst.h"
#include "testidkind.h"
#include "ui/idkind.h"
#include "ui/interface/iwindow.h"
#include "ui/pubsub/subscribe.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/rowkind.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/menu.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/eventtype.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace {

using PureGlUi::NO_FOCUS;
using PureGlUi::NO_FOCUS_LOSS;
using PureGlUi::NO_WINDOW;
using PureGlUi::NOTHING;
using PureGlUi::SHIFT;

class MenuBarWindowChangeTest : public ::testing::Test {
protected:
    std::string                m_read; // every event the loop read, for a failure to say
    std::unique_ptr<Ui::Shell> m_shell;
    std::vector<Ui::id_t>      m_menuIds;
    int                        m_passes = 0; // passes the loop ran

    void TearDown() override
    {
        if (m_shell) {
            m_shell->shutdown();
        }
    }

    // The shell up and its first frame drawn, as a user first sees it, nothing waited for.
    // False where there is no window, or no title opens a list
    [[nodiscard]] bool start()
    {
        auto shell = std::make_unique<Ui::Shell>();
        if (!shell->initialize()) {
            return false;
        }
        m_shell   = std::move(shell);
        m_menuIds = PureGlUi::listMenuIdsOf(*m_shell);
        PureGlUi::traceEvents(*m_shell, m_read);
        m_shell->windowManager().renderAll();
        return !m_menuIds.empty();
    }

    // A shell before this one that never ran its loop - made, drawn, a list opened and shut
    // down, as TestSecondMainWindow's - change done to its window before it ends. False
    // where there is no window
    template <typename change_t>
    [[nodiscard]] static bool runShellBefore(change_t change)
    {
        Ui::Shell shell;
        if (!shell.initialize()) {
            return false;
        }
        const std::vector<Ui::id_t> menuIds = PureGlUi::listMenuIdsOf(shell);
        shell.windowManager().renderAll();
        if (!menuIds.empty()) {
            shell.openPopup(menuIds.front());
        }
        change(shell.windowManager().mainWindow());
        shell.shutdown();
        return true;
    }

    [[nodiscard]] Ui::Window::NativeWindow & mainWindow() { return m_shell->windowManager().mainWindow(); }

    // The list open, INVALID_ID for none
    [[nodiscard]] Ui::id_t openMenuId() const { return m_shell->resManager().activeMenuId(); }

    // Action done in the loop's first pass, the loop run until it stops or its passes end
    template <typename action_t>
    void inFirstPass(action_t action)
    {
        PureGlUi::runPasses(
        [this, &action](int pass) {
            m_passes = pass + 1;
            if (pass == 0) {
                action();
            }
        },
        *m_shell);
    }

    // A press on the first title in the loop's first pass, before and after done to the
    // window in that pass round it; the list open once the loop has run
    template <typename before_t, typename after_t>
    [[nodiscard]] Ui::id_t pressFirstTitle(before_t before, after_t after)
    {
        inFirstPass([this, &before, &after] {
            before();
            PureGlUi::postAt(*m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
            after();
        });
        return openMenuId();
    }

    void openDialog()
    {
        Ui::Res::Type::dialog_t dialog;
        dialog.title  = "focus";
        dialog.width  = 300.0F;
        dialog.height = 200.0F;
        m_shell->windowManager().openDialog(dialog);
    }

#ifdef __APPLE__
    // The main window key, as it is for a user at it
    [[nodiscard]] bool makeKey() { return activate() && PureGlUi::hasKeyWindow(mainWindow()); }

    // A click on Quit: AppKit offered it, the shell's close path read it, and the loop ended
    void expectQuitCloses()
    {
        bool isOffered = false;
        inFirstPass([&isOffered] { isOffered = PureGlUi::clickQuit(); });
        EXPECT_TRUE(isOffered) << "Quit is greyed: " << m_read;
        EXPECT_NE(m_read.find("close;"), std::string::npos) << m_read;
        EXPECT_LT(m_passes, PureGlUi::PASSES) << m_read;
    }
#endif

    // The app active, as a user's is with anything of its open. False where the OS gives it none
    [[nodiscard]] bool activate()
    {
        PureGlUi::settle(mainWindow());
        return PureGlUi::setActive(mainWindow(), true);
    }

    // The app loses the focus, the loop reads that, and it has the focus back. False where the
    // OS kept it
    [[nodiscard]] bool loseFocus()
    {
        if (!PureGlUi::setActive(mainWindow(), false)) {
            return false;
        }
        PureGlUi::runPasses([](int /*pass*/) { }, *m_shell);
        (void)PureGlUi::setActive(mainWindow(), true);
        return true;
    }

    // A dock row whose menu is the first title's list, open as its chevron opens it
    void openRowMenu()
    {
        const Ui::id_t       rowId = Ui::idOf(PureGlUi::TestIdKind::Row, 1);
        Ui::Res::Dock::row_t row;
        row.id    = rowId;
        row.label = "menu";
        row.kind  = Ui::Res::Dock::RowKind::Menu;
        row.menu  = m_shell->resManager().findMenuItem(m_menuIds.at(0)).key;
        m_shell->windowManager().setDockRows(m_shell->resManager().layout().docks.front().name, { row });
        m_shell->openRowMenu(rowId);
    }

    // The size the shell lays out and draws at is the one the OS gave its window
    void expectTheOsSize()
    {
        const Ui::Res::Type::bound_t held  = mainWindow().bound();
        const Ui::Res::Type::bound_t given = PureGlUi::nativeContentRectOf(mainWindow());
        EXPECT_FLOAT_EQ(held.w, given.w) << m_read;
        EXPECT_FLOAT_EQ(held.h, given.h) << m_read;
    }
};

// TestMenuBar's first case as it runs after TestSecondMainWindow: a press on a settled
// window made after a shell that never ran its loop
TEST_F(MenuBarWindowChangeTest, APressAfterAShellThatNeverRanOpensItsList)
{
    if (!runShellBefore([](const Ui::IWindow & /*window*/) { }) || !start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    EXPECT_EQ(pressFirstTitle(NOTHING, NOTHING), m_menuIds.at(0)) << m_read;
}

// The close button pressed on the window of a shell that never read it: the next shell's
// loop runs on, the request not its own
TEST_F(MenuBarWindowChangeTest, AShellRunsOnAfterOneWhoseWindowWasAskedToClose)
{
    if (!runShellBefore(PureGlUi::postCloseRequest) || !start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    EXPECT_EQ(pressFirstTitle(NOTHING, NOTHING), m_menuIds.at(0)) << m_read;
    EXPECT_EQ(m_passes, PureGlUi::PASSES) << m_read;
}

// The window of a shell that never read it resized: the next shell holds the size the OS
// gave its own window, not that one
TEST_F(MenuBarWindowChangeTest, AShellHoldsItsOwnSizeAfterOneWhoseWindowWasResized)
{
    const auto shrink = [](const Ui::IWindow & window) {
        PureGlUi::moveResizeBy(window, { .w = -SHIFT, .h = -SHIFT });
    };
    if (!runShellBefore(shrink) || !start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::runPasses([](int /*pass*/) { }, *m_shell);
    expectTheOsSize();
}

// The window moved and shown there, as a user sees it before clicking, then a press queued
// at the title where it stands now: the loop reads the move and the press in one batch,
// settle() reading neither
TEST_F(MenuBarWindowChangeTest, APressInTheBatchOfAMoveOpensItsList)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    const auto move = [this] {
        PureGlUi::moveResizeBy(mainWindow(), { .x = SHIFT, .y = SHIFT });
        PureGlUi::settle(mainWindow());
    };
    EXPECT_EQ(pressFirstTitle(move, NOTHING), m_menuIds.at(0)) << m_read;
}

// The window resized and shown so, then a press queued at the title where it stands now
TEST_F(MenuBarWindowChangeTest, APressInTheBatchOfAResizeOpensItsList)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    const auto shrink = [this] {
        PureGlUi::moveResizeBy(mainWindow(), { .w = -SHIFT, .h = -SHIFT });
        PureGlUi::settle(mainWindow());
    };
    EXPECT_EQ(pressFirstTitle(shrink, NOTHING), m_menuIds.at(0)) << m_read;
}

// A press queued, then the window moved before the loop reads it: the press was made on
// the title, wherever the window went after
TEST_F(MenuBarWindowChangeTest, APressQueuedBeforeAMoveOpensItsList)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    const auto move = [this] { PureGlUi::moveResizeBy(mainWindow(), { .x = SHIFT, .y = SHIFT }); };
    EXPECT_EQ(pressFirstTitle(NOTHING, move), m_menuIds.at(0)) << m_read;
}

// A press queued, then the window resized before the loop reads it: macOS keeps a frame's
// bottom-left, so there its top edge moves
TEST_F(MenuBarWindowChangeTest, APressQueuedBeforeAResizeOpensItsList)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    const auto shrink = [this] { PureGlUi::moveResizeBy(mainWindow(), { .w = -SHIFT, .h = -SHIFT }); };
    EXPECT_EQ(pressFirstTitle(NOTHING, shrink), m_menuIds.at(0)) << m_read;
}

// TestMenuBar's premise: a window settle() let go of stays where it is while the loop runs,
// or a press queued after it is read against a frame since moved
TEST_F(MenuBarWindowChangeTest, ASettledWindowStaysWhereItIs)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    const Ui::Res::Type::bound_t settled = PureGlUi::nativeContentRectOf(mainWindow());
    PureGlUi::runPasses([](int /*pass*/) { }, *m_shell);
    const Ui::Res::Type::bound_t ran = PureGlUi::nativeContentRectOf(mainWindow());
    EXPECT_FLOAT_EQ(ran.x, settled.x) << m_read;
    EXPECT_FLOAT_EQ(ran.y, settled.y) << m_read;
    EXPECT_FLOAT_EQ(ran.w, settled.w) << m_read;
    EXPECT_FLOAT_EQ(ran.h, settled.h) << m_read;
}

// The OS may give a window another size than asked - AppKit fits one taller than its
// screen below the menu bar - and the shell lays it out and draws it at that size from its
// first frame, not from the first event that says so
TEST_F(MenuBarWindowChangeTest, TheShellHoldsTheSizeTheOsGaveItsWindow)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    expectTheOsSize();
}

// A list open as its app loses the focus closes, as every OS's own menu does. The app has
// the focus first, as a user's has while a list is open: one that never had it loses none
TEST_F(MenuBarWindowChangeTest, AListClosesAsTheAppLosesTheFocus)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    PureGlUi::settle(mainWindow());
    if (!PureGlUi::setActive(mainWindow(), true)) {
        GTEST_SKIP() << NO_FOCUS;
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));
    if (!PureGlUi::setActive(mainWindow(), false)) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    PureGlUi::runPasses([](int /*pass*/) { }, *m_shell);
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    (void)PureGlUi::setActive(mainWindow(), true);
}

// The focus lost in the batch a pointer starts a switch in, the old list gone and the next
// waiting for its frame: none opens over the app that took the focus. Lost from the move's
// own event, the switch just started there, since this order is all the case is about
TEST_F(MenuBarWindowChangeTest, AListSwitchUnderWayStopsAsTheAppLosesTheFocus)
{
    if (!start() || m_menuIds.size() < 2) {
        GTEST_SKIP() << "fewer than two titles open a list here";
    }
    PureGlUi::settle(mainWindow());
    if (!PureGlUi::setActive(mainWindow(), true)) {
        GTEST_SKIP() << NO_FOCUS;
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));

    bool                    hasSwitchStarted = false;
    bool                    isFocusLost      = false;
    Ui::PubSub::Subscribe & subscribe        = m_shell->windowManager().subscribe();
    const Ui::id_t          subscriber       = Ui::idOf(PureGlUi::TestIdKind::Subscriber, 1);
    subscribe.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::MouseMove),
                  subscriber,
                  [this, &hasSwitchStarted, &isFocusLost] {
                      if (!hasSwitchStarted && !m_shell->windowManager().hasPopup()) {
                          hasSwitchStarted = true;
                          isFocusLost      = PureGlUi::setActive(mainWindow(), false);
                      }
                  });
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 2) {
            PureGlUi::postAt(*m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
        }
    },
    *m_shell);
    subscribe.remove(subscriber);
    (void)PureGlUi::setActive(mainWindow(), true);

    ASSERT_TRUE(hasSwitchStarted) << "the pointer started no switch: " << m_read;
    if (!isFocusLost) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
}

// Negative: a dialog stays for the user's return, the same window and not one made again
TEST_F(MenuBarWindowChangeTest, ADialogStaysAsTheAppLosesTheFocus)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    openDialog();
    Ui::Window::WindowManager & windowManager = m_shell->windowManager();
    ASSERT_NE(windowManager.dialogWindow(), nullptr);
    const Ui::id_t idBefore = windowManager.dialogWindow()->subscribeId();
    if (!loseFocus()) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_NE(m_read.find("focuslost;"), std::string::npos) << m_read;
    ASSERT_NE(windowManager.dialogWindow(), nullptr) << m_read;
    EXPECT_EQ(windowManager.dialogWindow()->subscribeId(), idBefore) << m_read;
}

// Negative: with nothing open there is nothing to close, and nothing appears
TEST_F(MenuBarWindowChangeTest, NothingOpenStaysThatWayAsTheAppLosesTheFocus)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    if (!loseFocus()) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_NE(m_read.find("focuslost;"), std::string::npos) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    EXPECT_FALSE(m_shell->windowManager().hasPopup()) << m_read;
    EXPECT_EQ(m_shell->windowManager().dialogWindow(), nullptr) << m_read;
}

TEST_F(MenuBarWindowChangeTest, AListAndItsSubmenuCloseAsTheAppLosesTheFocus)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    Ui::id_t menuId   = Ui::INVALID_ID;
    Ui::id_t parentId = Ui::INVALID_ID;
    for (const Ui::id_t id : m_menuIds) {
        for (const Ui::Res::Type::menu_t & item : m_shell->resManager().findMenuItem(id).items) {
            if (parentId == Ui::INVALID_ID && !item.items.empty()) {
                menuId   = id;
                parentId = item.id;
            }
        }
    }
    if (parentId == Ui::INVALID_ID) {
        GTEST_SKIP() << "no list here has a submenu";
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    Ui::Window::WindowManager & windowManager = m_shell->windowManager();
    m_shell->openPopup(menuId);
    windowManager.reopenSubmenu(parentId);
    ASSERT_TRUE(windowManager.hasSubmenu());
    if (!loseFocus()) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_FALSE(windowManager.hasSubmenu()) << m_read;
    EXPECT_FALSE(windowManager.hasPopup()) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
}

TEST_F(MenuBarWindowChangeTest, ARowsMenuClosesAsTheAppLosesTheFocus)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    openRowMenu();
    ASSERT_TRUE(m_shell->windowManager().hasPopup());
    if (!loseFocus()) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_FALSE(m_shell->windowManager().hasPopup()) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
}

// A row's menu keeps the bar from switching while it is open, so one left marked open would
// stop a list following the pointer from then on
TEST_F(MenuBarWindowChangeTest, AListFollowsThePointerAfterARowsMenuClosedThisWay)
{
    if (!start() || m_menuIds.size() < 2) {
        GTEST_SKIP() << "fewer than two titles open a list here";
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    openRowMenu();
    ASSERT_TRUE(m_shell->windowManager().hasPopup());
    bool isFocusLost = false;
    PureGlUi::runPasses(
    [this, &isFocusLost](int pass) {
        if (pass == 1) {
            isFocusLost = PureGlUi::setActive(mainWindow(), false);
        } else if (pass == 3) {
            // The press alone: were the row's menu still open over the title, a release would run
            // the item under it, and a native dialog would hold the whole run
            (void)PureGlUi::setActive(mainWindow(), true);
            PureGlUi::postAt(*m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
        } else if (pass == 5) {
            PureGlUi::postAt(*m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
        }
    },
    *m_shell);
    if (!isFocusLost) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_EQ(openMenuId(), m_menuIds.at(1)) << m_read;
}

// A press on the title whose list the focus loss closed opens it: one left marked open would
// read as a press to close it
TEST_F(MenuBarWindowChangeTest, APressOpensTheListTheFocusLossClosed)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));
    bool isFocusLost = false;
    PureGlUi::runPasses(
    [this, &isFocusLost](int pass) {
        if (pass == 1) {
            isFocusLost = PureGlUi::setActive(mainWindow(), false);
        } else if (pass == 3) {
            (void)PureGlUi::setActive(mainWindow(), true);
            PureGlUi::postAt(*m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
        }
    },
    *m_shell);
    if (!isFocusLost) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_NE(m_read.find("focuslost;"), std::string::npos) << m_read;
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
}

// Negative: a hover over the bar opens nothing once the focus loss has closed the list, as
// with no list open it has nothing to switch from
TEST_F(MenuBarWindowChangeTest, AHoverOpensNothingAfterTheFocusLossClosedTheList)
{
    if (!start() || m_menuIds.size() < 2) {
        GTEST_SKIP() << "fewer than two titles open a list here";
    }
    if (!activate()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));
    bool isFocusLost = false;
    PureGlUi::runPasses(
    [this, &isFocusLost](int pass) {
        if (pass == 1) {
            isFocusLost = PureGlUi::setActive(mainWindow(), false);
        } else if (pass == 3) {
            (void)PureGlUi::setActive(mainWindow(), true);
            PureGlUi::postAt(*m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
        }
    },
    *m_shell);
    if (!isFocusLost) {
        GTEST_SKIP() << NO_FOCUS_LOSS;
    }
    EXPECT_NE(m_read.find("focuslost;"), std::string::npos) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    EXPECT_FALSE(m_shell->windowManager().hasPopup()) << m_read;
}

#ifdef __APPLE__

// The yellow button, Cmd+M and the Window menu's Minimize all ask this of AppKit
TEST_F(MenuBarWindowChangeTest, AWindowCanBeMinimized)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    EXPECT_TRUE(PureGlUi::minimize(mainWindow()));
}

TEST_F(MenuBarWindowChangeTest, QuitClosesTheWindow)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    expectQuitCloses();
}

TEST_F(MenuBarWindowChangeTest, QuitClosesTheWindowWithAListOpen)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));
    expectQuitCloses();
}

TEST_F(MenuBarWindowChangeTest, QuitClosesTheWindowWithADialogOpen)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    openDialog();
    ASSERT_NE(m_shell->windowManager().dialogWindow(), nullptr);
    expectQuitCloses();
}

// AppKit offers Quit here, yet performClose: sent its way closed nothing
TEST_F(MenuBarWindowChangeTest, QuitClosesAMinimizedWindow)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    if (!PureGlUi::minimize(mainWindow())) {
        GTEST_SKIP() << PureGlUi::NO_MINIMIZE;
    }
    expectQuitCloses();
}

// Both ways at once: the shortcut table's Ctrl + Q, Cmd folding into Ctrl, and the menu's Quit
TEST_F(MenuBarWindowChangeTest, CommandQEndsTheLoop)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    inFirstPass([] { PureGlUi::postQ(NSEventModifierFlagCommand); });
    EXPECT_NE(m_read.find("key Ctrl+q;"), std::string::npos) << m_read;
    EXPECT_LT(m_passes, PureGlUi::PASSES) << m_read;
}

// Minimized the shell reads no such press, a window not its own having it, so the menu's Quit ends it
TEST_F(MenuBarWindowChangeTest, CommandQEndsTheLoopWithTheWindowMinimized)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    if (!PureGlUi::minimize(mainWindow())) {
        GTEST_SKIP() << PureGlUi::NO_MINIMIZE;
    }
    inFirstPass([] { PureGlUi::postQ(NSEventModifierFlagCommand); });
    EXPECT_NE(m_read.find("close;"), std::string::npos) << m_read;
    EXPECT_LT(m_passes, PureGlUi::PASSES) << m_read;
}

// Negative: a q typed alone ends nothing
TEST_F(MenuBarWindowChangeTest, AQAloneEndsNothing)
{
    if (!start()) {
        GTEST_SKIP() << NO_WINDOW;
    }
    if (!makeKey()) {
        GTEST_SKIP() << NO_FOCUS;
    }
    inFirstPass([] { PureGlUi::postQ(0); });
    EXPECT_NE(m_read.find("key q;"), std::string::npos) << m_read;
    EXPECT_EQ(m_passes, PureGlUi::PASSES) << m_read;
    EXPECT_EQ(m_read.find("close;"), std::string::npos) << m_read;
}

#endif

} // namespace

#endif
