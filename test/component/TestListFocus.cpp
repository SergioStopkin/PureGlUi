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
 * @file TestListFocus.cpp
 * @brief An open list stays open as its OS gives it the focus, which is still its app's
 *
 * One case on all four platforms: each moves the focus its own way (nativefocus.h) and says
 * its own way that the app lost it. X11 says so per window, and a list that is a window of
 * root is a sibling of the main window, so there a window manager focusing it as it maps
 * reads as another app taking the focus. Win32 asks which process is in front, macOS and
 * Wayland the app's own state. Every failure says what the loop read
 */

#include "menubarloop.h"
#include "nativefocus.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/config.h"
#include "ui/res/type/dialog.h"
#include "ui/shell.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>

namespace {

using ListFocusTest = PureGlUi::ShellTest;

TEST_F(ListFocusTest, AListStaysOpenAsTheOsGivesItTheFocus)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    if (Ui::g_config.isCompositing) {
        GTEST_SKIP() << "a list here is never mapped, drawn into the main frame instead";
    }
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    PureGlUi::makeListsTopLevel(mainWindow());
    if (!PureGlUi::setActive(mainWindow(), true)) {
        GTEST_SKIP() << PureGlUi::NO_FOCUS;
    }
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(windowManager.popupWindow(), nullptr) << m_read;
    const Ui::Window::Popup::PopupWindow & list = *windowManager.popupWindow();
    if (list.followsParent()) {
        GTEST_SKIP() << "no ARGB visual here: a list is the main window's child, and shares its focus";
    }
    if (!PureGlUi::giveFocus(list)) {
        GTEST_SKIP() << "the OS keeps the focus from a list here";
    }
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
}

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(HAVE_WAYLAND)

// Where no 32-bit visual has an EGL config a list is the main window's child: the focus moving
// into it stays inside the app, which X11 says with NotifyInferior
TEST_F(ListFocusTest, AListStaysOpenAsTheFocusMovesIntoItsChildWindow)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    if (Ui::g_config.isCompositing) {
        GTEST_SKIP() << "a list here is never mapped, drawn into the main frame instead";
    }
    if (!PureGlUi::setActive(mainWindow(), true)) {
        GTEST_SKIP() << PureGlUi::NO_FOCUS;
    }
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    const Ui::Window::Popup::PopupWindow & list = *m_shell.windowManager().popupWindow();
    if (!list.followsParent()) {
        GTEST_SKIP() << "a list here is a window of root, not the main window's child";
    }
    if (!PureGlUi::focusOn(list)) {
        GTEST_SKIP() << "the OS keeps the focus from a list here";
    }
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
}

// A window manager focusing a list as it maps would read as the app losing the focus
TEST_F(ListFocusTest, AListRefusesTheFocus)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_TRUE(PureGlUi::refusesFocus(*m_shell.windowManager().popupWindow()));
}

// A dialog stays while the app is away, and a click on it coming back has to give it the keys
TEST_F(ListFocusTest, ADialogDoesNotRefuseTheFocus)
{
    Ui::Res::Type::dialog_t dialog;
    dialog.title  = "focus";
    dialog.width  = 300.0F;
    dialog.height = 200.0F;
    m_shell.windowManager().openDialog(dialog);
    ASSERT_NE(m_shell.windowManager().dialogWindow(), nullptr) << m_read;
    EXPECT_FALSE(PureGlUi::refusesFocus(*m_shell.windowManager().dialogWindow()));
}

#endif

} // namespace
