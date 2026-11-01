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
 * @file TestScaleChange.cpp
 * @brief The UI follows its window's DPI as the OS changes it, with no restart
 *
 * The OS says the window stands at another DPI the way it does when the window moves to
 * another monitor or the desktop's scale setting changes - Win32's WM_DPICHANGED, X11's
 * Xft.dpi rewritten in the root window's resources (nativegeometry.h) - and the shell's own
 * loop lays the UI out again at it: the scale every conversion reads, PUREGLUI_SCALE's
 * factor still on it, and an open list made again at its new size. Every failure says what
 * the loop read
 *
 * Skipped on macOS, where only another screen gives a window another backing scale. Not on
 * native Wayland, whose scale is its output's
 */

#if defined(_WIN32) || defined(__APPLE__) || (defined(HAVE_X11) && !defined(HAVE_WAYLAND))

#include "menubarloop.h"
#include "nativegeometry.h"
#include "ui/config.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/popupwindow.h"

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace {

class ScaleChangeTest : public ::testing::Test {
protected:
    std::string           m_read; // every event the loop read, for a failure to say
    Ui::Shell             m_shell;
    bool                  m_isInitialized = false;
    std::vector<Ui::id_t> m_menuIds;
    int                   m_dpiBefore    = 0; // the window's, put back for the tests after
    bool                  m_isDpiChanged = false;

    void SetUp() override
    {
        if (!m_shell.initialize()) {
            GTEST_SKIP() << "no window or GL context here";
        }
        m_isInitialized = true;
        m_menuIds       = PureGlUi::listMenuIdsOf(m_shell);
        if (m_menuIds.empty()) {
            GTEST_SKIP() << "no title opens a list here";
        }
        m_dpiBefore = mainWindow().dpi();
        PureGlUi::traceEvents(m_shell, m_read);
        m_shell.windowManager().renderAll();
        PureGlUi::settle(mainWindow());
    }

    void TearDown() override
    {
        if (m_isDpiChanged) {
            (void)PureGlUi::setDpi(mainWindow(), m_dpiBefore);
        }
        if (m_isInitialized) {
            m_shell.shutdown();
        }
    }

    [[nodiscard]] Ui::Window::NativeWindow & mainWindow() { return m_shell.windowManager().mainWindow(); }
};

// Half as large again as the window stood: laid out at that DPI, an open list made again
// at it
TEST_F(ScaleChangeTest, TheUiFollowsTheDpiItsOsGives)
{
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr);
    const Ui::fpx_t listWidth = m_shell.windowManager().popupWindow()->bound().w;
    const Ui::fpx_t scale     = Ui::g_config.scale;

    const int changed = m_dpiBefore * 3 / 2;
    if (!PureGlUi::setDpi(mainWindow(), changed)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses(m_shell, [](int /*pass*/) { });

    ASSERT_EQ(mainWindow().dpi(), changed) << "the window does not say the DPI its OS gave it";
    EXPECT_EQ(Ui::g_config.dpi, m_shell.windowManager().askedDpi()) << m_read;
    EXPECT_NEAR(Ui::g_config.scale, scale * 1.5F, 0.02F) << m_read;
    EXPECT_EQ(m_shell.resManager().activeMenuId(), m_menuIds.at(0)) << m_read;
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_NEAR(m_shell.windowManager().popupWindow()->bound().w, listWidth * 1.5F, 3.0F) << m_read;
}

} // namespace

#endif
