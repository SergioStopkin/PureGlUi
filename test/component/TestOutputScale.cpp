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
 * Native Wayland only, which ./build.sh ct wl builds: X11 has no buffer scale, and its px stay
 */

#ifdef HAVE_WAYLAND

#include "menubarloop.h"
#include "proxyof.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/config.h"
#include "ui/pubsub/subscribe.h"
#include "ui/res/type/bound.h"
#include "ui/shell.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/eventfactory.h"
#include "ui/window/eventtype.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>
#include <wayland-client.h>

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

    // A child made while the UI's scale stands at uiScale, as PUREGLUI_SCALE puts a factor on
    // the output's, and put back once it is made
    bool createChildAt(Ui::fpx_t uiScale, Ui::Window::NativeWindow & child)
    {
        const Ui::fpx_t scaleBefore = Ui::g_config.scale;
        Ui::g_config.scale          = uiScale;
        const bool isCreated        = child.create(PureGlUi::CHILD_WIDTH,
                                                   PureGlUi::CHILD_HEIGHT,
                                                   mainWindow().nativeDisplay(),
                                                   mainWindow().nativeHandle(),
                                                   "");
        Ui::g_config.scale          = scaleBefore;
        return isCreated;
    }

    // A pointer motion at (x, y) surface px fed to a handler of the test's own, as the compositor
    // sends one, while the UI's scale stands at uiScale: the move it reads back, none for no pointer
    [[nodiscard]] Ui::Window::Event pointerMovedAt(Ui::fpx_t uiScale, int x, int y)
    {
        Ui::Window::NativeEvent events;
        events.init(mainWindow());
        wl_pointer * pointer = events.wlPointer();
        if (pointer == nullptr) {
            return {};
        }
        wl_proxy *      proxy       = PureGlUi::proxyOf(pointer);
        const auto *    listener    = static_cast<const wl_pointer_listener *>(wl_proxy_get_listener(proxy));
        const Ui::fpx_t scaleBefore = Ui::g_config.scale;
        Ui::g_config.scale          = uiScale;
        listener->motion(wl_proxy_get_user_data(proxy), pointer, 0, wl_fixed_from_int(x), wl_fixed_from_int(y));
        Ui::g_config.scale = scaleBefore;
        Ui::Window::Event event;
        while (events.pollEvent(event, mainWindow())) {
            if (event.type == Ui::Window::EventType::MouseMove) {
                return event;
            }
        }
        return {};
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

// A content surface's buffer is made at the output's scale already, so the output's scale,
// arriving as it is made, is no change: it stands the size it was asked for
TEST_F(OutputScaleTest, AChildWindowStandsTheSizeItWasMadeAt)
{
    if (m_scaleBefore == 1) {
        GTEST_SKIP() << "the output's scale is 1 here, the one a window starts at";
    }
    Ui::PubSub::Subscribe    subscribe;
    Ui::Window::NativeWindow child(subscribe);
    ASSERT_TRUE(createChildAt(Ui::g_config.scale, child));
    EXPECT_FLOAT_EQ(child.bound().w, PureGlUi::CHILD_WIDTH) << "at scale " << m_scaleBefore;
    EXPECT_FLOAT_EQ(child.bound().h, PureGlUi::CHILD_HEIGHT) << "at scale " << m_scaleBefore;
}

// A content surface's size is its parent's layout's, given again at the new scale as the shell
// lays it out: its own output's scale arriving after that is its buffer's alone, its size kept
TEST_F(OutputScaleTest, AChildWindowKeepsTheSizeItWasGivenAsItsOutputsScaleChanges)
{
    Ui::PubSub::Subscribe    subscribe;
    Ui::Window::NativeWindow child(subscribe);
    ASSERT_TRUE(createChildAt(Ui::g_config.scale, child));
    // The room the layout gives it at twice the scale, before its output says so
    const Ui::Res::Type::bound_t given { .x = 0.0F,
                                         .y = 0.0F,
                                         .w = PureGlUi::CHILD_WIDTH * 2.0F,
                                         .h = PureGlUi::CHILD_HEIGHT * 2.0F };
    child.moveResize(given);
    child.handleOutputScale(m_scaleBefore * 2);
    EXPECT_FLOAT_EQ(child.bound().w, given.w) << "at scale " << m_scaleBefore * 2;
    EXPECT_FLOAT_EQ(child.bound().h, given.h) << "at scale " << m_scaleBefore * 2;
}

// PUREGLUI_SCALE is the UI's, never a buffer's: a child made under a factor above 1 stands the
// size it was asked for, its buffer at its output's scale
TEST_F(OutputScaleTest, AChildWindowMadeUnderAFactorAbove1StandsTheSizeItWasMadeAt)
{
    Ui::PubSub::Subscribe    subscribe;
    Ui::Window::NativeWindow child(subscribe);
    ASSERT_TRUE(createChildAt(Ui::g_config.scale * 2.0F, child));
    EXPECT_FLOAT_EQ(child.bound().w, PureGlUi::CHILD_WIDTH) << "at scale " << m_scaleBefore;
    EXPECT_FLOAT_EQ(child.bound().h, PureGlUi::CHILD_HEIGHT) << "at scale " << m_scaleBefore;
    EXPECT_EQ(child.dpi(), mainWindow().dpi());
}

// The pointer where the compositor puts it, its surface px times the window's buffer scale, the
// output's: PUREGLUI_SCALE is the UI's, and a press read at another place lands on another element
TEST_F(OutputScaleTest, APointerUnderAScaleFactorLandsWhereItIs)
{
    const Ui::Window::Event moved = pointerMovedAt(Ui::g_config.scale * 2.0F, 100, 50);
    ASSERT_EQ(moved.type, Ui::Window::EventType::MouseMove) << "no pointer here";
    EXPECT_EQ(moved.mouse.x, 100 * m_scaleBefore) << "at scale " << m_scaleBefore;
    EXPECT_EQ(moved.mouse.y, 50 * m_scaleBefore) << "at scale " << m_scaleBefore;
}

// Negative: with no factor the UI's scale is the output's, and the pointer lands there already
TEST_F(OutputScaleTest, APointerWithNoScaleFactorLandsWhereItIs)
{
    const Ui::Window::Event moved = pointerMovedAt(Ui::g_config.scale, 100, 50);
    ASSERT_EQ(moved.type, Ui::Window::EventType::MouseMove) << "no pointer here";
    EXPECT_EQ(moved.mouse.x, 100 * m_scaleBefore) << "at scale " << m_scaleBefore;
    EXPECT_EQ(moved.mouse.y, 50 * m_scaleBefore) << "at scale " << m_scaleBefore;
}

// A list opened under a factor stands at its output's scale, as the main window does: its size
// and place in surface px are its px over that scale, never over the UI's
TEST_F(OutputScaleTest, AListOpenedUnderAScaleFactorStandsAtItsOutputsScale)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    const Ui::fpx_t scaleBefore = Ui::g_config.scale;
    Ui::g_config.scale          = scaleBefore * 2.0F;
    m_shell.openPopup(m_menuIds.at(0));
    Ui::g_config.scale = scaleBefore;
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_EQ(m_shell.windowManager().popupWindow()->dpi(), mainWindow().dpi());
}

// A list open as its output's scale changes is made again at it, the same list: left as it was it
// would stand at the old scale, half or twice its size on screen
TEST_F(OutputScaleTest, AListOpenAsTheOutputsScaleChangesIsMadeAgainAtIt)
{
    if (m_menuIds.empty()) {
        GTEST_SKIP() << "no title opens a list here";
    }
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 1) {
            m_shell.openPopup(m_menuIds.at(0));
        } else if (pass == 2) {
            mainWindow().handleOutputScale(m_scaleBefore * 2);
        }
    },
    m_shell);
    EXPECT_EQ(mainWindow().dpi(), m_scaleBefore * 2 * 96) << m_read;
    EXPECT_EQ(openMenuId(), m_menuIds.at(0)) << m_read;
    ASSERT_NE(m_shell.windowManager().popupWindow(), nullptr) << m_read;
    EXPECT_EQ(m_shell.windowManager().popupWindow()->dpi(), mainWindow().dpi()) << m_read;
}

// A popup - a list, a dialog, the tooltip - made under a factor above 1: the size it asks in
// surface px and the configure's answer are its px over its buffer's scale, never the UI's, so
// it stands the size it was made at
TEST_F(OutputScaleTest, APopupMadeUnderAScaleFactorStandsTheSizeItWasMadeAt)
{
    Ui::PubSub::Subscribe          subscribe;
    Ui::Window::Popup::PopupWindow popup(subscribe);
    const Ui::fpx_t                scaleBefore = Ui::g_config.scale;
    Ui::g_config.scale                         = scaleBefore * 2.0F;
    const bool isCreated = popup.create(mainWindow(), PureGlUi::CHILD_WIDTH, PureGlUi::CHILD_HEIGHT);
    Ui::g_config.scale   = scaleBefore;
    ASSERT_TRUE(isCreated);
    EXPECT_FLOAT_EQ(popup.bound().w, PureGlUi::CHILD_WIDTH) << "at scale " << m_scaleBefore;
    EXPECT_FLOAT_EQ(popup.bound().h, PureGlUi::CHILD_HEIGHT) << "at scale " << m_scaleBefore;
    EXPECT_EQ(popup.dpi(), mainWindow().dpi());
}

// The same under a factor below 1
TEST_F(OutputScaleTest, AChildWindowMadeUnderAFactorBelow1StandsTheSizeItWasMadeAt)
{
    if (m_scaleBefore == 1) {
        GTEST_SKIP() << "a factor below 1 truncates to the output's scale of 1 here, which is no change";
    }
    Ui::PubSub::Subscribe    subscribe;
    Ui::Window::NativeWindow child(subscribe);
    ASSERT_TRUE(createChildAt(Ui::g_config.scale / 2.0F, child));
    EXPECT_FLOAT_EQ(child.bound().w, PureGlUi::CHILD_WIDTH) << "at scale " << m_scaleBefore;
    EXPECT_FLOAT_EQ(child.bound().h, PureGlUi::CHILD_HEIGHT) << "at scale " << m_scaleBefore;
    EXPECT_EQ(child.dpi(), mainWindow().dpi());
}

} // namespace

#endif
