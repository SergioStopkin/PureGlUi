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
 * @file TestTabDrag.cpp
 * @brief A tab dragged through the shell's own loop, the pointer put in the OS's queue as the
 *        OS puts it: past the window's left edge, where a held button still reports the
 *        pointer, it is read left of the window - Win32 packs it in 16 bits - so the tab goes left;
 *        and a tab moved glides to its place with no event, the loop drawing each frame of it
 *
 * X11 and Win32: where a test puts a pointer left of the window in the window's own queue
 */

#if defined(_WIN32) || defined(HAVE_X11)

#include "common/time.h"
#include "menubarloop.h"
#include "nativeinput.h"
#include "shelltest.h"
#include "testconst.h"
#include "testidkind.h"
#include "ui/animation/durationof.h"
#include "ui/config.h"
#include "ui/idkind.h"
#include "ui/res/key/element.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/tab.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {

constexpr Ui::id_t  LEFT           = Ui::idOf(PureGlUi::TestIdKind::Tab, 1);
constexpr Ui::id_t  MIDDLE         = Ui::idOf(PureGlUi::TestIdKind::Tab, 2); // the one dragged
constexpr Ui::id_t  RIGHT          = Ui::idOf(PureGlUi::TestIdKind::Tab, 3);
constexpr int       PAST_LEFT_EDGE = -10;   // physical px left of the window
constexpr Ui::fpx_t NUDGE          = 10.0F; // CSS px: past the click threshold, short of a swap

class TabDragTest : public PureGlUi::ShellTest {
protected:
    std::vector<std::pair<Ui::id_t, Ui::id_t>> m_moves;     // each jump: the tab, the one it went before
    std::vector<Ui::id_t>                      m_activated; // each tab a click activated

    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        m_shell.setOnTabMoved([this](Ui::id_t id, Ui::id_t beforeId) { m_moves.emplace_back(id, beforeId); });
        m_shell.setOnTabActivated([this](Ui::id_t id) { m_activated.emplace_back(id); });
        m_shell.resManager().tabBar().setTabs(
        { Ui::tab_t { .id = LEFT, .isActive = true }, Ui::tab_t { .id = MIDDLE }, Ui::tab_t { .id = RIGHT } });
        m_shell.windowManager().requestContentRefresh();
    }
};

} // namespace

// Pressed on the middle tab and dragged past the left edge: before its left neighbour, never to
// the end of the strip
TEST_F(TabDragTest, ATabDraggedPastTheLeftEdgeGoesLeft)
{
    bool isOnStrip = false;
    PureGlUi::runPasses(
    [this, &isOnStrip](int pass) {
        // The first pass lays the tabs out
        if (pass != 1) {
            return;
        }
        const Ui::Res::Type::bound_t middle = m_shell.windowManager().elementBound(MIDDLE);
        isOnStrip                           = middle.w > 0;
        const int y                         = Ui::roundToInt(Ui::toPhys(middle.y + (middle.h / 2.0F)));
        PureGlUi::postPointerPress(mainWindow(), Ui::roundToInt(Ui::toPhys(middle.x + (middle.w / 2.0F))), y);
        PureGlUi::postPointerMove(mainWindow(), PAST_LEFT_EDGE, y);
        PureGlUi::postPointerRelease(mainWindow(), PAST_LEFT_EDGE, y);
    },
    m_shell);
    ASSERT_TRUE(isOnStrip) << "the tab was not laid out; " << m_read;
    EXPECT_EQ(m_moves, (std::vector<std::pair<Ui::id_t, Ui::id_t>> { { MIDDLE, LEFT } })) << m_read;
}

// Dragged a little and let go over itself: no swap, and no click - a drag never activates a tab
TEST_F(TabDragTest, ADragReleasedOnATabIsNoClick)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass != 1) {
            return;
        }
        const Ui::Res::Type::bound_t middle = m_shell.windowManager().elementBound(MIDDLE);
        const Ui::fpx_t              x      = middle.x + (middle.w / 2.0F);
        const int                    y      = Ui::roundToInt(Ui::toPhys(middle.y + (middle.h / 2.0F)));
        PureGlUi::postPointerPress(mainWindow(), Ui::roundToInt(Ui::toPhys(x)), y);
        PureGlUi::postPointerMove(mainWindow(), Ui::roundToInt(Ui::toPhys(x + NUDGE)), y);
        PureGlUi::postPointerRelease(mainWindow(), Ui::roundToInt(Ui::toPhys(x + NUDGE)), y);
    },
    m_shell);
    EXPECT_TRUE(m_moves.empty()) << m_read;
    EXPECT_TRUE(m_activated.empty()) << m_read;
}

// Let go short of the click threshold, a press on a tab is its click: the tab is activated
TEST_F(TabDragTest, APressShortOfTheThresholdIsStillAClick)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass != 1) {
            return;
        }
        const Ui::Res::Type::bound_t middle = m_shell.windowManager().elementBound(MIDDLE);
        const int                    x      = Ui::roundToInt(Ui::toPhys(middle.x + (middle.w / 2.0F)));
        const int                    y      = Ui::roundToInt(Ui::toPhys(middle.y + (middle.h / 2.0F)));
        PureGlUi::postPointerPress(mainWindow(), x, y);
        PureGlUi::postPointerRelease(mainWindow(), x, y);
    },
    m_shell);
    EXPECT_TRUE(m_moves.empty()) << m_read;
    EXPECT_EQ(m_activated, (std::vector<Ui::id_t> { MIDDLE })) << m_read;
}

#ifdef HAVE_X11

// The pointer drifting off the strip into a child window - a content surface - is still over the
// app, so the drag goes on: the move after it still jumps the tab left. X tells the main window
// it left, for its child (NotifyInferior)
TEST_F(TabDragTest, ALeaveIntoAChildWindowKeepsTheDrag)
{
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass != 1) {
            return;
        }
        const Ui::Res::Type::bound_t middle = m_shell.windowManager().elementBound(MIDDLE);
        const Ui::Res::Type::bound_t left   = m_shell.windowManager().elementBound(LEFT);
        const int                    x      = Ui::roundToInt(Ui::toPhys(middle.x + (middle.w / 2.0F)));
        const int                    y      = Ui::roundToInt(Ui::toPhys(middle.y + (middle.h / 2.0F)));
        const int                    leftX  = Ui::roundToInt(Ui::toPhys(left.x));
        PureGlUi::postPointerPress(mainWindow(), x, y);
        PureGlUi::postPointerLeaveIntoChild(mainWindow(), x, y);
        PureGlUi::postPointerMove(mainWindow(), leftX, y);
        PureGlUi::postPointerRelease(mainWindow(), leftX, y);
    },
    m_shell);
    EXPECT_EQ(m_moves, (std::vector<std::pair<Ui::id_t, Ui::id_t>> { { MIDDLE, LEFT } })) << m_read;
}

#endif

// A tab moved glides to its new place with no event to draw it on: the loop asks a frame of its
// own each pass while one moves (WindowManager::keepGliding), so the glide lands
TEST_F(TabDragTest, AMovedTabGlidesToItsPlaceWithNoEvents)
{
    const Common::Time::span_t glide = Ui::Animation::durationOf(
    m_shell.resManager().transitionsFor(Ui::Res::Key::ElementKey::WorkspaceTab));
    bool wasGliding = false;
    PureGlUi::runPasses(
    [this, &glide, &wasGliding](int pass) {
        // The first pass draws the tabs where they stand; the next moves two of them
        if (pass == 1) {
            m_shell.resManager().tabBar().setTabs(
            { Ui::tab_t { .id = MIDDLE }, Ui::tab_t { .id = LEFT, .isActive = true }, Ui::tab_t { .id = RIGHT } });
            m_shell.windowManager().requestContentRefresh();
        } else if (pass == 2) {
            wasGliding = m_shell.windowManager().isMainGliding();
            Common::Time::sleepFor(glide + PureGlUi::GLIDE_MARGIN);
        }
    },
    m_shell);
    ASSERT_TRUE(wasGliding) << "the move started no glide; " << m_read;
    EXPECT_FALSE(m_shell.windowManager().isMainGliding()) << "the glide stopped short of its end; " << m_read;
}

#endif
