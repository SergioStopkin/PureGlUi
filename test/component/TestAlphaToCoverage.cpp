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
 * @file TestAlphaToCoverage.cpp
 * @brief A list's and a dialog's text draws as the main window's: blended, never also made
 *        sample coverage, which takes every glyph edge's coverage a second time and reads thin
 *
 * X11 and native Wayland, the two whose popups turned it on
 */

#if defined(HAVE_X11) || defined(HAVE_WAYLAND)

#include "shelltest.h"
#include "ui/gl/localglew.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/dialog.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>

namespace {

class AlphaToCoverageTest : public PureGlUi::ShellTest {
protected:
    // Whether a window's own context turns each fragment's alpha into sample coverage
    [[nodiscard]] bool isAlphaToCoverageOnIn(Ui::IWindow & window)
    {
        window.makeCurrent();
        const bool isOn = glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE) == GL_TRUE;
        mainWindow().makeCurrent();
        return isOn;
    }

    // Whether a popup's context could turn it on: multisampled, and on X11 with an alpha visual
    // too. Off those a pass says nothing
    [[nodiscard]] static bool isCoverageWithin(const Ui::Window::Popup::PopupWindow & window)
    {
#ifdef HAVE_X11
        return window.msaaVisual() && window.hasAlpha();
#else
        return window.msaaVisual();
#endif
    }
};

TEST_F(AlphaToCoverageTest, AListBlendsWithoutAlphaToCoverage)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    m_shell.openPopup(m_menuIds.at(0));
    Ui::Window::Popup::PopupWindow * list = m_shell.windowManager().popupWindow();
    ASSERT_NE(list, nullptr) << m_read;
    if (!isCoverageWithin(*list)) {
        GTEST_SKIP() << "a list here gets no config it could be turned on in";
    }
    EXPECT_FALSE(isAlphaToCoverageOnIn(*list));
}

TEST_F(AlphaToCoverageTest, ADialogBlendsWithoutAlphaToCoverage)
{
    Ui::Res::Type::dialog_t dialog;
    dialog.title  = "coverage";
    dialog.width  = 300.0F;
    dialog.height = 200.0F;
    m_shell.windowManager().openDialog(dialog);
    Ui::Window::Popup::DialogWindow * dialogWindow = m_shell.windowManager().dialogWindow();
    ASSERT_NE(dialogWindow, nullptr) << m_read;
    if (!isCoverageWithin(*dialogWindow)) {
        GTEST_SKIP() << "a dialog here gets no config it could be turned on in";
    }
    EXPECT_FALSE(isAlphaToCoverageOnIn(*dialogWindow));
}

// Negative: the main window never turned it on, and its text already drew as it should
TEST_F(AlphaToCoverageTest, TheMainWindowBlendsWithoutAlphaToCoverage)
{
    EXPECT_FALSE(isAlphaToCoverageOnIn(mainWindow()));
}

} // namespace

#endif
