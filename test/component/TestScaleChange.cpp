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
 * word that the root window's resources were rewritten, the new DPI itself coming through
 * PUREGLUI_SCALE so a test never writes the desktop's (nativegeometry.h) - and the shell's own
 * loop lays the UI out again at it: the scale every conversion reads, PUREGLUI_SCALE's
 * factor still on it, and an open list made again at its new size. Every failure says what
 * the loop read
 *
 * A window placed where another DPI awaits it, as a session's restore places it, has the size
 * it was asked for: those px are already that monitor's, and scaled again they are saved and
 * scaled again by the next launch
 *
 * A reload is worth a change of DPI: one per batch, none where the DPI stayed, and each later
 * change followed in turn
 *
 * Skipped on macOS, where only another screen gives a window another backing scale. Not on
 * native Wayland, whose scale is its output's
 */

#if defined(_WIN32) || defined(__APPLE__) || defined(HAVE_X11)

#include "menubarloop.h"
#include "nativegeometry.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/config.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/menu.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <cmath>
#include <cstddef>
#include <gtest/gtest.h>
#include <string>
#include <utility>
#include <vector>

namespace {

using PureGlUi::PROJECTED_ROW;
using PureGlUi::SHIFT;

class ScaleChangeTest : public PureGlUi::ShellTest {
protected:
    int  m_dpiBefore    = 0; // the window's, put back for the tests after
    bool m_isDpiChanged = false;
    int  m_reloads      = 0; // the reloads the shell has finished

    void SetUp() override
    {
        m_hooks.afterReload = [this] { ++m_reloads; };
        ShellTest::SetUp();
        if (!m_isInitialized) {
            return;
        }
        if (m_menuIds.empty()) {
            GTEST_SKIP() << "no title opens a list here";
        }
        m_dpiBefore = mainWindow().dpi();
        PureGlUi::settle(mainWindow());
    }

    void TearDown() override
    {
        if (m_isDpiChanged) {
            PureGlUi::restoreDpi(mainWindow(), m_dpiBefore);
        }
        ShellTest::TearDown();
    }

    void openDialog()
    {
        Ui::Res::Type::dialog_t dialog;
        dialog.title  = "scale";
        dialog.width  = 300.0F;
        dialog.height = 200.0F;
        m_shell.windowManager().openDialog(dialog);
    }

    // Rows into the first dock, open at its default width as a double-click on its grip opens
    // it, laid out in a frame drawn now: none clears it. With no session a dock starts closed
    void projectRows(std::vector<Ui::Res::Dock::row_t> rows)
    {
        const Ui::Res::Dock::dock_config_t & dock = m_shell.resManager().layout().docks.front();
        m_shell.resManager().setDockState(dock.name, { .width = dock.defaultWidth, .memoryX = dock.defaultWidth });
        m_shell.windowManager().apply(Ui::Res::Type::changes_t(Ui::Res::Type::Changed::Layout));
        m_shell.windowManager().setDockRows(dock.name, std::move(rows));
        m_shell.windowManager().renderAll();
    }

    [[nodiscard]] static Ui::Res::Dock::row_t projectedRow()
    {
        Ui::Res::Dock::row_t row;
        row.id    = PROJECTED_ROW;
        row.label = "projected";
        return row;
    }

    [[nodiscard]] bool isRowShown() { return m_shell.windowManager().elementBound(PROJECTED_ROW).w > 0.0F; }
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
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);

    EXPECT_EQ(Ui::g_config.dpi, m_shell.windowManager().askedDpi()) << m_read;
    EXPECT_NEAR(Ui::g_config.scale, scale * 1.5F, 0.02F) << m_read;
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_NEAR(m_shell.windowManager().popupWindow()->bound().w, listWidth * 1.5F, 3.0F) << m_read;
}

// At the size it has, to a lower DPI: the minimum size is in CSS px, so it holds at both, where
// a higher DPI raises it past the size asked and the OS rightly keeps to it
TEST_F(ScaleChangeTest, AWindowPlacedAtAnotherDpiHasTheSizeItWasAsked)
{
    const Ui::Res::Type::bound_t now = mainWindow().bound();
    const Ui::Res::Type::bound_t asked { .x = now.x + SHIFT, .y = now.y + SHIFT, .w = now.w, .h = now.h };
    if (!PureGlUi::placeAtAnotherDpi(asked, mainWindow(), m_dpiBefore * 2 / 3)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);

    const Ui::Res::Type::bound_t given = PureGlUi::nativeContentRectOf(mainWindow());
    EXPECT_FLOAT_EQ(given.w, asked.w) << m_read;
    EXPECT_FLOAT_EQ(given.h, asked.h) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(asked.w)) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowHeight(), Ui::roundToInt(asked.h)) << m_read;
}

// Negative: nothing happened, so nothing is laid out again
TEST_F(ScaleChangeTest, NoChangeReloadsNothing)
{
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(m_reloads, 0) << m_read;
}

TEST_F(ScaleChangeTest, AChangeOfDpiReloadsOnce)
{
    if (!PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(m_reloads, 1) << m_read;
}

// A reload that never let go of its wait would leave every later change unfollowed
TEST_F(ScaleChangeTest, ALaterChangeOfDpiReloadsAgain)
{
    if (!PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 3) {
            (void)PureGlUi::setDpi(mainWindow(), m_dpiBefore * 2);
        }
    },
    m_shell);
    EXPECT_EQ(m_reloads, 2) << m_read;
}

// The OS can reload the UI at any moment, as Ctrl+R only did when asked: a dialog comes back
// made again, as large to the eye, on a whole CSS px
TEST_F(ScaleChangeTest, ADialogOpenAcrossAChangeOfDpiIsOpenAgainAtItsNewSize)
{
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    openDialog();
    ASSERT_NE(windowManager.dialogWindow(), nullptr);
    const Ui::id_t               idBefore = windowManager.dialogWindow()->subscribeId();
    const Ui::Res::Type::bound_t before   = windowManager.dialogWindow()->bound();

    if (!PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);

    ASSERT_NE(windowManager.dialogWindow(), nullptr) << m_read;
    EXPECT_NE(windowManager.dialogWindow()->subscribeId(), idBefore) << m_read;
    const Ui::Res::Type::bound_t after = windowManager.dialogWindow()->bound();
    EXPECT_NEAR(after.w, before.w * 1.5F, 3.0F) << m_read;
    EXPECT_NEAR(after.h, before.h * 1.5F, 3.0F) << m_read;
    EXPECT_FLOAT_EQ(after.x, Ui::toPhysEdge(std::round(Ui::toCss(after.x)))) << m_read;
    EXPECT_FLOAT_EQ(after.y, Ui::toPhysEdge(std::round(Ui::toCss(after.y)))) << m_read;
}

// A list, its submenu and the item the pointer was on come back too, by key: an item's id
// starts over with every load of the menus
TEST_F(ScaleChangeTest, ASubmenuOpenAcrossAChangeOfDpiIsOpenAgainWithItsItem)
{
    Ui::id_t menuId   = Ui::INVALID_ID;
    Ui::id_t parentId = Ui::INVALID_ID;
    Ui::id_t childId  = Ui::INVALID_ID;
    // An item a pointer can be on: no highlight lands on a disabled one, which is any with no
    // handler, nor on a separator
    for (const Ui::id_t id : m_menuIds) {
        for (const Ui::Res::Type::menu_t & item : m_shell.resManager().findMenuItem(id).items) {
            for (const Ui::Res::Type::menu_t & child : item.items) {
                if (childId == Ui::INVALID_ID && child.enabled && child.visible && !child.separator) {
                    menuId   = id;
                    parentId = item.id;
                    childId  = child.id;
                }
            }
        }
    }
    if (childId == Ui::INVALID_ID) {
        GTEST_SKIP() << "no list here has a submenu with an enabled item";
    }
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    m_shell.openPopup(menuId);
    windowManager.reopenSubmenu(parentId);
    ASSERT_TRUE(windowManager.hasSubmenu());
    windowManager.setActivePopupItem(childId);
    ASSERT_EQ(windowManager.activePopupItemId(), childId);
    const Ui::key_t key   = m_shell.resManager().findMenuItem(childId).key;
    const Ui::fpx_t width = windowManager.submenuWindow()->bound().w;

    if (!PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);

    // A real display reports its pointer as the window opens under it, and a pointer on the list
    // rightly closes a submenu it left: no case posts one here, so this one proves nothing then
    if (m_read.find(" move ") != std::string::npos) {
        GTEST_SKIP() << "the display's pointer moved over the window: " << m_read;
    }
    EXPECT_EQ(openMenuId(), menuId) << m_read;
    ASSERT_TRUE(windowManager.hasSubmenu()) << m_read;
    EXPECT_EQ(m_shell.resManager().findMenuItem(windowManager.activePopupItemId()).key, key) << m_read;
    EXPECT_NEAR(windowManager.submenuWindow()->bound().w, width * 1.5F, 3.0F) << m_read;
}

// Negative: a reload puts back what was open, and only that
TEST_F(ScaleChangeTest, NothingOpenAcrossAChangeOfDpiStaysThatWay)
{
    if (!PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2)) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(m_reloads, 1) << m_read;
    EXPECT_EQ(openMenuId(), Ui::INVALID_ID) << m_read;
    EXPECT_EQ(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_EQ(m_shell.windowManager().dialogWindow(), nullptr) << m_read;
}

// A reload builds every dock afresh, and a host is told of no change of DPI: the rows it
// projected are the framework's to keep
// One loop, as a stop is for good: its first pass places the docks, the next projects the row
// and changes the DPI
TEST_F(ScaleChangeTest, ADocksRowsAreBackAfterAChangeOfDpi)
{
    ASSERT_FALSE(m_shell.resManager().layout().docks.empty()) << "the shipped layout declares no dock";
    bool wasShown = false;
    PureGlUi::runPasses(
    [this, &wasShown](int pass) {
        if (pass == 1) {
            projectRows({ projectedRow() });
            wasShown       = isRowShown();
            m_isDpiChanged = PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2);
        }
    },
    m_shell);
    if (!m_isDpiChanged) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    ASSERT_TRUE(wasShown) << "the row projected never stood in the layout: " << m_read;
    EXPECT_EQ(m_reloads, 1) << m_read;
    EXPECT_TRUE(isRowShown()) << m_read;
}

// A change of DPI builds no dock, its layout's CSS px the same; a theme switch or a res edit
// builds every one afresh, and the rows are back unasked
TEST_F(ScaleChangeTest, ADocksRowsAreBackAfterItsDockIsBuiltAgain)
{
    ASSERT_FALSE(m_shell.resManager().layout().docks.empty()) << "the shipped layout declares no dock";
    bool wasShown = false;
    PureGlUi::runPasses(
    [this, &wasShown](int pass) {
        if (pass == 1) {
            projectRows({ projectedRow() });
            wasShown = isRowShown();
            m_shell.windowManager().apply(Ui::Res::Type::changes_t(Ui::Res::Type::Changed::Layout));
            m_shell.windowManager().renderAll();
        }
    },
    m_shell);
    ASSERT_TRUE(wasShown) << "the row projected never stood in the layout: " << m_read;
    EXPECT_TRUE(isRowShown()) << m_read;
}

// Negative: a row the host took back is not brought back
TEST_F(ScaleChangeTest, ADocksRowsTakenBackStayGoneAfterAChangeOfDpi)
{
    ASSERT_FALSE(m_shell.resManager().layout().docks.empty()) << "the shipped layout declares no dock";
    bool wasShown     = false;
    bool wasTakenBack = false;
    PureGlUi::runPasses(
    [this, &wasShown, &wasTakenBack](int pass) {
        if (pass == 1) {
            projectRows({ projectedRow() });
            wasShown = isRowShown();
            projectRows({});
            wasTakenBack   = !isRowShown();
            m_isDpiChanged = PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2);
        }
    },
    m_shell);
    if (!m_isDpiChanged) {
        GTEST_SKIP() << "the OS gives a window no other DPI here";
    }
    ASSERT_TRUE(wasShown) << "the row projected never stood in the layout: " << m_read;
    ASSERT_TRUE(wasTakenBack) << "the row taken back still stood in the layout: " << m_read;
    EXPECT_EQ(m_reloads, 1) << m_read;
    EXPECT_FALSE(isRowShown()) << m_read;
}

#if !defined(_WIN32) && !defined(__APPLE__)

// X11 alone says the resources were rewritten whatever they now say, which is what these
// cases need: a rewrite with no DPI of its own, and more than one event in a batch

// The scale events the loop read, so a case cannot pass on having read none
[[nodiscard]] int scaleEventsIn(const std::string & read)
{
    int count = 0;
    for (std::size_t at = read.find("scale;"); at != std::string::npos; at = read.find("scale;", at + 1)) {
        ++count;
    }
    return count;
}

// Negative: any xrdb -merge rewrites them, and that is no reason to close a dialog and restyle
TEST_F(ScaleChangeTest, ARewriteAtTheSameDpiReloadsNothing)
{
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore));
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(scaleEventsIn(m_read), 1) << m_read;
    EXPECT_EQ(m_reloads, 0) << m_read;
}

// Negative: a dialog left alone by a rewrite is the same window, not one made again in its place
TEST_F(ScaleChangeTest, ADialogStaysTheSameWindowThroughARewriteAtTheSameDpi)
{
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    openDialog();
    ASSERT_NE(windowManager.dialogWindow(), nullptr);
    const Ui::id_t idBefore = windowManager.dialogWindow()->subscribeId();
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore));
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(scaleEventsIn(m_read), 1) << m_read;
    ASSERT_NE(windowManager.dialogWindow(), nullptr) << m_read;
    EXPECT_EQ(windowManager.dialogWindow()->subscribeId(), idBefore) << m_read;
}

// Negative: nor is a list made again
TEST_F(ScaleChangeTest, AListStaysTheSameWindowThroughARewriteAtTheSameDpi)
{
    Ui::Window::WindowManager & windowManager = m_shell.windowManager();
    m_shell.openPopup(m_menuIds.at(0));
    ASSERT_NE(windowManager.popupWindow(), nullptr);
    const Ui::id_t idBefore = windowManager.popupWindow()->subscribeId();
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore));
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(scaleEventsIn(m_read), 1) << m_read;
    ASSERT_NE(windowManager.popupWindow(), nullptr) << m_read;
    EXPECT_EQ(windowManager.popupWindow()->subscribeId(), idBefore) << m_read;
}

// Negative: the batch ends where it began
TEST_F(ScaleChangeTest, AChangeThereAndBackInOneBatchReloadsNothing)
{
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2));
    m_isDpiChanged = true;
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore));
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(scaleEventsIn(m_read), 2) << m_read;
    EXPECT_EQ(m_reloads, 0) << m_read;
}

TEST_F(ScaleChangeTest, TwoChangesInOneBatchReloadOnce)
{
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2));
    m_isDpiChanged = true;
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore * 2));
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(scaleEventsIn(m_read), 2) << m_read;
    EXPECT_EQ(m_reloads, 1) << m_read;
}

// Xft.dpi is the desktop's, which a test only reads: one that wrote it, stopped there, would
// leave the wrong scale behind for the desktop's other apps and every run after
TEST_F(ScaleChangeTest, ATestLeavesTheDesktopsDpiAlone)
{
    ASSERT_TRUE(PureGlUi::setDpi(mainWindow(), m_dpiBefore * 3 / 2));
    m_isDpiChanged = true;
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_EQ(mainWindow().dpi(), m_dpiBefore) << m_read;
    EXPECT_EQ(m_reloads, 1) << m_read;
}

#endif

} // namespace

#endif
