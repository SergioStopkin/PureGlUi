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
 * @brief A press on a title opens its list whatever its window was just doing, and an open
 * list closes as its app loses the focus
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
#include "nativegeometry.h"
#include "nativeinput.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/nativewindow.h"

#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

namespace {

// Physical px a window is moved or resized by: past half a title, so a press read against
// the frame before the change lands off it
constexpr Ui::fpx_t SHIFT = 120.0F;

constexpr const char * NO_WINDOW = "no window, or no title opens a list here";

// Nothing done to the window round a press
constexpr auto NOTHING = [] { };

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

    // A press on the first title in the loop's first pass, before and after done to the
    // window in that pass round it; the list open once the loop has run
    template <typename before_t, typename after_t>
    [[nodiscard]] Ui::id_t pressFirstTitle(before_t before, after_t after)
    {
        PureGlUi::runPasses(*m_shell, [this, &before, &after](int pass) {
            m_passes = pass + 1;
            if (pass == 0) {
                before();
                PureGlUi::postAt(*m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
                after();
            }
        });
        return openMenuId();
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
    PureGlUi::runPasses(*m_shell, [](int /*pass*/) { });
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
    PureGlUi::runPasses(*m_shell, [](int /*pass*/) { });
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
        GTEST_SKIP() << "the OS gives the app no focus here";
    }
    m_shell->openPopup(m_menuIds.at(0));
    ASSERT_EQ(openMenuId(), m_menuIds.at(0));
    if (!PureGlUi::setActive(mainWindow(), false)) {
        GTEST_SKIP() << "the OS kept the app active here";
    }
    PureGlUi::runPasses(*m_shell, [](int /*pass*/) { });
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    (void)PureGlUi::setActive(mainWindow(), true);
}

} // namespace

#endif
