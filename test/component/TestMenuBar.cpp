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
 * input put in the OS's event queue (nativeinput.h), since the batches are the loop's
 *
 * Not on native Wayland, where no client puts pointer events in its own queue
 */

#if defined(_WIN32) || defined(__APPLE__) || (defined(HAVE_X11) && !defined(HAVE_WAYLAND))

#include "nativegeometry.h"
#include "nativeinput.h"
#include "ui/config.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/menu.h"
#include "ui/shell.h"
#include "ui/type.h"

#include <functional>
#include <gtest/gtest.h>
#include <vector>

namespace {

// Passes the loop runs: the ones a test scripts, then several with nothing new, for
// anything the shell still owes the pointer
constexpr int PASSES = 8;

class MenuBarTest : public ::testing::Test {
protected:
    Ui::Shell             m_shell;
    bool                  m_isInitialized = false;
    std::vector<Ui::id_t> m_menuIds;

    void SetUp() override
    {
        if (!m_shell.initialize()) {
            GTEST_SKIP() << "no window or GL context here";
        }
        m_isInitialized = true;
        for (const Ui::Res::Type::menu_t & menu : m_shell.resManager().menus()) {
            if (menu.visible && menu.enabled && menu.actionKey.empty() && !menu.items.empty()) {
                m_menuIds.emplace_back(menu.id);
            }
        }
        if (m_menuIds.size() < 3) {
            GTEST_SKIP() << "fewer than three menus open a list here";
        }

        // On screen and drawn, as a user first sees it, before any input: AppKit placing a
        // window just made reads a posted event's position against where it stood before
        m_shell.windowManager().renderAll();
        PureGlUi::settle(m_shell.windowManager().mainWindow());
    }

    void TearDown() override
    {
        if (m_isInitialized) {
            m_shell.shutdown();
        }
    }

    // The shell's own loop, script called at the start of each pass with its number
    void runPasses(const std::function<void(int)> & script)
    {
        int pass = 0;
        m_shell.setFrameTasks([this, &pass, &script] {
            script(pass);
            ++pass;
            if (pass >= PASSES) {
                m_shell.requestStop();
            }
        });
        m_shell.run();
    }

    // Pointer input at the middle of a menu's button, where the next poll finds it
    template <typename post_t>
    void postAt(Ui::id_t menuId, post_t post)
    {
        const Ui::Res::Type::bound_t & button = m_shell.windowManager().elementBound(menuId);
        post(m_shell.windowManager().mainWindow(),
             Ui::roundToInt(Ui::toPhys(button.x + (button.w / 2.0F))),
             Ui::roundToInt(Ui::toPhys(button.y + (button.h / 2.0F))));
    }

    // The list open once the loop has run, INVALID_ID for none
    [[nodiscard]] Ui::id_t openMenuId() const { return m_shell.resManager().activeMenuId(); }
};

// A title's list shows as the button goes down on it
TEST_F(MenuBarTest, APressOpensItsList)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            postAt(m_menuIds.at(0), PureGlUi::postPointerPress);
        }
    });
    EXPECT_EQ(openMenuId(), m_menuIds.at(0));
}

// A click made while the pointer sweeps on: pressed on the first title, the button comes
// up over the next - the pressed list opens, then follows the pointer to where it rests
TEST_F(MenuBarTest, AClickWhileSweepingOpensThePressedListThenFollows)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            postAt(m_menuIds.at(0), PureGlUi::postPointerPress);
            postAt(m_menuIds.at(1), PureGlUi::postPointerMove);
            postAt(m_menuIds.at(1), PureGlUi::postPointerRelease);
        }
    });
    EXPECT_EQ(openMenuId(), m_menuIds.at(1));
}

// A quick click, press and release on the one title, leaves its list open: the release
// takes back nothing the press did
TEST_F(MenuBarTest, AQuickClickLeavesItsListOpen)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            postAt(m_menuIds.at(0), PureGlUi::postPointerPress);
            postAt(m_menuIds.at(0), PureGlUi::postPointerRelease);
        }
    });
    EXPECT_EQ(openMenuId(), m_menuIds.at(0));
}

// A press on the open list's title closes it, before its button comes up
TEST_F(MenuBarTest, APressOnTheOpenTitleClosesItsList)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            postAt(m_menuIds.at(0), PureGlUi::postPointerPress);
        }
    });
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID);
}

// A whole click on the open list's title closes it and leaves it closed: the release
// after the closing press opens nothing again
TEST_F(MenuBarTest, AClickOnTheOpenTitleLeavesItsListClosed)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            postAt(m_menuIds.at(0), PureGlUi::postPointerPress);
            postAt(m_menuIds.at(0), PureGlUi::postPointerRelease);
        }
    });
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID);
}

// A click opens a list and the pointer is on the next title by the same batch of
// events, while that list is just made: the next title's list opens
TEST_F(MenuBarTest, APointerOnTheNextTitleByTheClickOpensItsList)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            // What the click on the first title does
            m_shell.openPopup(m_menuIds.at(0));
            postAt(m_menuIds.at(1), PureGlUi::postPointerMove);
        }
    });
    EXPECT_EQ(openMenuId(), m_menuIds.at(1));
}

// The pointer crosses a title and is on the one after it by one batch, while the switch
// to the title crossed still waits for its frame: the list the pointer is on opens, not
// the one it crossed
TEST_F(MenuBarTest, AListFollowsAPointerSweepingPastATitle)
{
    runPasses([this](int pass) {
        if (pass == 0) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 1) {
            // On the open list's own title: a pass with events, which ends the wait a
            // list just made holds a switch off for
            postAt(m_menuIds.at(0), PureGlUi::postPointerMove);
        } else if (pass == 2) {
            postAt(m_menuIds.at(1), PureGlUi::postPointerMove);
            postAt(m_menuIds.at(2), PureGlUi::postPointerMove);
        }
    });
    EXPECT_EQ(openMenuId(), m_menuIds.at(2));
}

} // namespace

#endif
