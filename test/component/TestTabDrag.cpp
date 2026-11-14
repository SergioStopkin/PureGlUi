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
 *        pointer, it is read left of the window - Win32 packs it in 16 bits - so the tab goes left
 *
 * X11 and Win32: where a test puts a pointer left of the window in the window's own queue
 */

#if defined(_WIN32) || defined(HAVE_X11)

#include "menubarloop.h"
#include "nativeinput.h"
#include "shelltest.h"
#include "testidkind.h"
#include "ui/config.h"
#include "ui/idkind.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/tab.h"
#include "ui/type.h"

#include <gtest/gtest.h>
#include <utility>
#include <vector>

namespace {

constexpr Ui::id_t LEFT           = Ui::idOf(PureGlUi::TestIdKind::Tab, 1);
constexpr Ui::id_t MIDDLE         = Ui::idOf(PureGlUi::TestIdKind::Tab, 2); // the one dragged
constexpr Ui::id_t RIGHT          = Ui::idOf(PureGlUi::TestIdKind::Tab, 3);
constexpr int      PAST_LEFT_EDGE = -10; // physical px left of the window

class TabDragTest : public PureGlUi::ShellTest {
protected:
    std::vector<std::pair<Ui::id_t, Ui::id_t>> m_moves; // each jump: the tab, the one it went before

    void SetUp() override
    {
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        m_shell.setOnTabMoved([this](Ui::id_t id, Ui::id_t beforeId) { m_moves.emplace_back(id, beforeId); });
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

#endif
