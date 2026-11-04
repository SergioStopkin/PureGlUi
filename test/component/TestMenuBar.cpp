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
 * @file TestMenuBar.cpp
 * @brief The menu bar behaves as every OS's and toolkit's own does
 *
 * doc/api/shell-actions.md "Menu bar behaviour" is the contract: a title opens its list on
 * the press, a quick click leaves it open, a press on the open title closes it, and an open
 * list follows the pointer across the titles however fast it moves. A fast pointer is the
 * hard part - a release landing past the title it pressed, a title reached in the same batch
 * of events as the click or while a switch still waits for its frame, and then a pointer at
 * rest that sends nothing more - so every case runs the shell's own loop, the pointer's
 * input put in the OS's event queue (nativeinput.h), since the batches are the loop's.
 * Every failure says what the loop read
 *
 * Not on native Wayland, where no client puts pointer events in its own queue
 */

#if defined(_WIN32) || defined(__APPLE__) || (defined(HAVE_X11) && !defined(HAVE_WAYLAND))

#include "menubarloop.h"
#include "nativegeometry.h"
#include "nativeinput.h"
#include "shelltest.h"
#include "ui/interface/iwindow.h"
#include "ui/shell.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <string>

namespace {

class MenuBarTest : public PureGlUi::ShellTest {
protected:
    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        if (m_menuIds.size() < 3) {
            GTEST_SKIP() << "fewer than three menus open a list here";
        }
        // Settled before any input: AppKit placing a window just made reads a posted event's
        // position against where it stood before
        PureGlUi::settle(mainWindow());
    }
};

// A title's list shows as the button goes down on it. A failure says what the OS says of the
// window as the press goes in and after: CI's macOS reads this one press 31 pt low, the
// process's first through AppKit's loop, on a window bigger than its screen
TEST_F(MenuBarTest, APressOpensItsList)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            PureGlUi::postAt(m_shell, m_menuIds.at(0), [this](const Ui::IWindow & window, int x, int y) {
                m_read += " posted " + std::to_string(x) + "," + std::to_string(y) + " with "
                        + PureGlUi::placementOf(window) + ";";
                PureGlUi::postPointerPress(window, x, y);
            });
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(0))
    << m_read << " then " << PureGlUi::placementOf(m_shell.windowManager().mainWindow());
}

// A click made while the pointer sweeps on: pressed on the first title, the button comes
// up over the next - the pressed list opens, then follows the pointer to where it rests
TEST_F(MenuBarTest, AClickWhileSweepingOpensThePressedListThenFollows)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
            PureGlUi::postAt(m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
            PureGlUi::postAt(m_shell, m_menuIds.at(1), PureGlUi::postPointerRelease);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(1)) << m_read;
}

// A quick click, press and release on the one title, leaves its list open: the release
// takes back nothing the press did
TEST_F(MenuBarTest, AQuickClickLeavesItsListOpen)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerRelease);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
}

// A press on the open list's title closes it, before its button comes up
TEST_F(MenuBarTest, APressOnTheOpenTitleClosesItsList)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
}

// A whole click on the open list's title closes it and leaves it closed: the release
// after the closing press opens nothing again
TEST_F(MenuBarTest, AClickOnTheOpenTitleLeavesItsListClosed)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerPress);
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerRelease);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
}

// A click opens a list and the pointer is on the next title by the same batch of
// events, while that list is just made: the next title's list opens
TEST_F(MenuBarTest, APointerOnTheNextTitleByTheClickOpensItsList)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            // What the click on the first title does
            m_shell.openPopup(m_menuIds.at(0));
            PureGlUi::postAt(m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(1)) << m_read;
}

// The pointer crosses a title and is on the one after it by one batch, while the switch
// to the title crossed still waits for its frame: the list the pointer is on opens, not
// the one it crossed
TEST_F(MenuBarTest, AListFollowsAPointerSweepingPastATitle)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 1) {
            // On the open list's own title: a pass with events, which ends the wait a
            // list just made holds a switch off for
            PureGlUi::postAt(m_shell, m_menuIds.at(0), PureGlUi::postPointerMove);
        } else if (pass == 2) {
            PureGlUi::postAt(m_shell, m_menuIds.at(1), PureGlUi::postPointerMove);
            PureGlUi::postAt(m_shell, m_menuIds.at(2), PureGlUi::postPointerMove);
        }
    },
    m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(2)) << m_read;
}

#ifdef __APPLE__

// A move off every window of the app stands on no title: AppKit gives it no window to scale
// by, so read as a position it is the window's corner, where the first title stands
TEST_F(MenuBarTest, APointerOffEveryWindowLeavesTheListOpen)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(1));
        } else if (pass == 2) {
            PureGlUi::postPointerMoveOffWindows();
        }
    },
    m_shell);
    EXPECT_NE(m_read.find("move -1,-1;"), std::string::npos) << m_read;
    EXPECT_EQ(openMenuId(), m_menuIds.at(1)) << m_read;
}

#endif

} // namespace

#endif
