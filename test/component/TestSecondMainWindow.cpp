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
 * @file TestSecondMainWindow.cpp
 * @brief A main window made after another was shut down, in the same process
 *
 * A main window's teardown terminates the EGL display its popups shared and drops the
 * config they were made from with it. Whatever a popup keeps across windows has to
 * follow, or the next main window - a test suite's next case, a host that starts its
 * shell again - can open no popup at all
 */

#include "isdisplayset.h"
#include "menubarloop.h"
#include "ui/shell.h"
#include "ui/type.h"

#ifdef __APPLE__
#include "nativefocus.h"

#include <iostream>
#endif

#include <gtest/gtest.h>
#include <vector>

namespace {

TEST(SecondMainWindow, OpensAMenuListAsTheFirstDid)
{
    for (int turn = 1; turn <= 2; ++turn) {
        Ui::Shell shell;
        if (!shell.initialize()) {
            ASSERT_FALSE(PureGlUi::isDisplaySet()) << PureGlUi::DISPLAY_UNREACHABLE;
            GTEST_SKIP() << "no window or GL context here";
        }
        const std::vector<Ui::id_t> menuIds = PureGlUi::listMenuIdsOf(shell);
        ASSERT_FALSE(menuIds.empty());

        // The frame the app draws before anyone can click a menu
        shell.windowManager().renderAll();
        shell.openPopup(menuIds.front());
        EXPECT_NE(shell.windowManager().popupWindow(), nullptr) << "main window " << turn;

        shell.shutdown();
    }
}

#ifdef __APPLE__

// A shell shut down takes its main window and its list with it, so each leaves AppKit as many
// windows as the one before it did - a window never closed stays listed until the process ends
TEST(SecondMainWindow, LeavesNoWindowOfItsOwnAsTheFirstDid)
{
    NSUInteger leftByFirst = 0;
    for (int turn = 1; turn <= 2; ++turn) {
        // Drained as an app's loop drains each pass: what AppKit autoreleased goes with the turn
        @autoreleasepool {
            Ui::Shell shell;
            if (!shell.initialize()) {
                GTEST_SKIP() << "no window or GL context here";
            }
            const std::vector<Ui::id_t> menuIds = PureGlUi::listMenuIdsOf(shell);
            ASSERT_FALSE(menuIds.empty());
            shell.windowManager().renderAll();
            shell.openPopup(menuIds.front());
            if (turn == 2) {
                EXPECT_GT(PureGlUi::appWindowCount(), leftByFirst) << "a shell up lists no window of its own";
            }
            shell.shutdown();
        }
        if (turn == 1) {
            leftByFirst = PureGlUi::appWindowCount();
            std::cout << "[Windows] left by the first: " << PureGlUi::appWindowsText() << std::endl;
        } else {
            EXPECT_EQ(PureGlUi::appWindowCount(), leftByFirst) << PureGlUi::appWindowsText();
        }
    }
}

#endif

} // namespace
