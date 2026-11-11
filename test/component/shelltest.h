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

#pragma once

#include "isdisplayset.h"
#include "menubarloop.h"
#include "ui/inithooks.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/nativewindow.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace PureGlUi {

// A shell up and its first frame drawn, as a user first sees it, every event its loop reads
// kept for a failure to say. A SetUp of its own checks m_isInitialized after this one, since
// a skip here ends only this call
class ShellTest : public ::testing::Test {
protected:
    std::string           m_read; // every event the loop read, for a failure to say
    Ui::Shell             m_shell;
    bool                  m_isInitialized = false;
    std::vector<Ui::id_t> m_menuIds; // the titles a click opens a list for
    Ui::init_hooks_t      m_hooks;   // what the shell starts with: set before this SetUp

    void SetUp() override
    {
        if (!m_shell.initialize(m_hooks)) {
            ASSERT_FALSE(isDisplaySet()) << DISPLAY_UNREACHABLE;
            GTEST_SKIP() << "no window or GL context here";
        }
        m_isInitialized = true;
        m_menuIds       = listMenuIdsOf(m_shell);
        traceEvents(m_shell, m_read);
        m_shell.windowManager().renderAll();
    }

    void TearDown() override
    {
        if (m_isInitialized) {
            m_shell.shutdown();
        }
    }

    [[nodiscard]] Ui::Window::NativeWindow & mainWindow() { return m_shell.windowManager().mainWindow(); }

    // The list open, INVALID_ID for none
    [[nodiscard]] Ui::id_t openMenuId() const { return m_shell.resManager().activeMenuId(); }
};

} // namespace PureGlUi
