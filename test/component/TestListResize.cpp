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
 * @file TestListResize.cpp
 * @brief An open list stays open as its window resizes, on native Wayland
 *
 * A resize hides a list and draws a frame before making the list again, and a hidden list's
 * frame swaps nothing, which would map it again. The compositor's resize comes as its configure
 * delivers it, and the first size the loop reads is one too. Every failure says what the loop read
 *
 * And a list hangs flush from its title, the window fullscreen or not: where a list stands is
 * ours, as on X11, and a compositor let move one keeps it out of what its desktop holds
 *
 * Native Wayland only, which ./build.sh ct wl builds: elsewhere a hidden window swaps for nothing
 */

#ifdef HAVE_WAYLAND

#include "menubarloop.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/config.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>

namespace {

using PureGlUi::PASSES;
using PureGlUi::SHIFT;

class ListResizeTest : public PureGlUi::ShellTest {
protected:
    // The window grown by SHIFT each way, as the compositor's configure says it in logical px
    void resizeAsTheCompositor()
    {
        const Ui::Res::Type::bound_t bound = mainWindow().bound();
        mainWindow().handleConfigure(Ui::roundToInt((bound.w + SHIFT) / mainWindow().bufferScale()),
                                     Ui::roundToInt((bound.h + SHIFT) / mainWindow().bufferScale()));
    }

    // The first title's list opened late in the loop, the window settled by then
    void openListLate(bool isFullscreen)
    {
        PureGlUi::runPasses(
        [this, isFullscreen](int pass) {
            if (pass == 0 && isFullscreen) {
                mainWindow().setFullscreen(true);
            } else if (pass == PASSES - 3) {
                m_shell.openPopup(m_menuIds.at(0));
            }
        },
        m_shell);
    }

    // Where the compositor stood the open list's top against its title's bottom: 0 is flush
    [[nodiscard]] Ui::fpx_t listDrop()
    {
        const Ui::Res::Type::bound_t & title = m_shell.windowManager().elementBound(m_menuIds.at(0));
        return m_shell.windowManager().popupWindow()->bound().y - Ui::toPhysEdge(title.y + title.h);
    }
};

// Opened before the loop ever ran: the first size it reads is a resize to it
TEST_F(ListResizeTest, AListStaysOpenAsTheWindowFirstReportsItsSize)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
    EXPECT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
}

// One loop, as a stop is for good: its first size read, then the list opened, then the resize
TEST_F(ListResizeTest, AListStaysOpenAsTheCompositorResizesTheWindow)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 1) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            resizeAsTheCompositor();
        }
    },
    m_shell);
    EXPECT_FLOAT_EQ(m_shell.windowManager().windowWidth(), widthBefore + SHIFT) << m_read;
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
    EXPECT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
}

TEST_F(ListResizeTest, AListStandsFlushUnderItsTitle)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    openListLate(false);
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_FLOAT_EQ(listDrop(), 0.0F) << m_read;
}

// Fullscreen the window covers what the desktop keeps for itself - GNOME's top bar - and a list
// the compositor may slide out of it stands lower than its title
TEST_F(ListResizeTest, AListStandsFlushUnderItsTitleFullscreen)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    openListLate(true);
    if (mainWindow().bound().w == widthBefore) {
        GTEST_SKIP() << "the compositor gave no fullscreen here";
    }
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_FLOAT_EQ(listDrop(), 0.0F) << m_read;
}

TEST_F(ListResizeTest, NoListOpensAsTheCompositorResizesTheWindow)
{
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 2) {
            resizeAsTheCompositor();
        }
    },
    m_shell);
    EXPECT_FLOAT_EQ(m_shell.windowManager().windowWidth(), widthBefore + SHIFT) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    EXPECT_EQ(m_shell.windowManager().popupWindow(), nullptr) << m_read;
}

} // namespace

#endif
