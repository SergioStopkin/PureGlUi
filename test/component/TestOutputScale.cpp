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
 * @file TestOutputScale.cpp
 * @brief A window keeps its size on screen as its output's scale changes, on native Wayland
 *
 * A surface stands on screen at its buffer's size over its buffer scale, so a buffer that does
 * not follow the scale takes the window with it. No client can change an output's scale, so the
 * change comes as the output's listener delivers it. A compositor that sends a configure after
 * it would have the window right by then; this asks for what needs none. Every failure says what
 * the loop read
 *
 * Native Wayland only, which build.sh never builds: X11 has no buffer scale, and its px stay
 */

#ifdef HAVE_WAYLAND

#include "menubarloop.h"
#include "shelltest.h"
#include "ui/config.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>

namespace {

class OutputScaleTest : public PureGlUi::ShellTest {
protected:
    int m_scaleBefore = 1; // the output's, put back for the tests after

    void SetUp() override
    {
        ShellTest::SetUp();
        if (m_isInitialized) {
            m_scaleBefore = mainWindow().dpi() / 96;
        }
    }

    void TearDown() override
    {
        if (m_isInitialized) {
            mainWindow().handleOutputScale(m_scaleBefore);
        }
        ShellTest::TearDown();
    }
};

TEST_F(OutputScaleTest, AWindowKeepsItsSizeOnScreenAsItsOutputsScaleChanges)
{
    const Ui::Res::Type::bound_t before = mainWindow().bound();
    mainWindow().handleOutputScale(m_scaleBefore * 2);
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);

    const Ui::Res::Type::bound_t after = mainWindow().bound();
    EXPECT_FLOAT_EQ(after.w, before.w * 2.0F) << m_read;
    EXPECT_FLOAT_EQ(after.h, before.h * 2.0F) << m_read;
    EXPECT_FLOAT_EQ(m_shell.windowManager().windowWidth(), after.w) << m_read;
    EXPECT_FLOAT_EQ(m_shell.windowManager().windowHeight(), after.h) << m_read;
    EXPECT_EQ(Ui::g_config.dpi, m_shell.windowManager().askedDpi()) << m_read;
}

} // namespace

#endif
