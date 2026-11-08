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
 * @file TestFullscreen.cpp
 * @brief A window that ends fullscreen starts fullscreen, on native Wayland
 *
 * Fullscreen there is a state a client asks for, never a size: a toplevel that is not is kept
 * inside the work area, below the desktop's top bar, however large it asks to be. So the
 * session keeps whether the window was, beside the geometry of the window it was before, for
 * making it windowed in a later run. Every failure says what the loop read
 *
 * Native Wayland, which ./build.sh ct wl builds: X11 comes back fullscreen by its size. And X11,
 * whose build shares the session file a native Wayland run may have ended fullscreen. And macOS,
 * whose fullscreen - the green button's, Ctrl+Cmd+F's, the Window menu's - is a state too
 */

#if defined(__APPLE__) || defined(HAVE_WAYLAND) || defined(HAVE_X11)

#include "menubarloop.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/config.h"
#include "ui/type.h"

#include <gtest/gtest.h>

#ifdef __APPLE__
#include "nativegeometry.h"
#elifdef HAVE_WAYLAND
#include "proxyof.h"

#include <cstdint>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#endif

namespace {

// Started from a session that ended fullscreen, as a session restored before the shell leaves it:
// the window it was beside it
class FullscreenRestoreTest : public PureGlUi::ShellTest {
protected:
    void SetUp() override
    {
        m_shell.resManager().setSessionWindowGeometry(0, 0, PureGlUi::RESTORED_WIDTH, PureGlUi::RESTORED_HEIGHT);
        m_shell.resManager().setSessionWindowFullscreen(true);
        ShellTest::SetUp();
    }
};

#ifdef __APPLE__

// Out of fullscreen again whatever the test did: the next test's window would start in its space
template <typename base_t>
class LeavesFullscreen : public base_t {
protected:
    void TearDown() override
    {
        if (this->m_isInitialized) {
            (void)PureGlUi::setEnlarged(this->mainWindow(), false);
        }
        base_t::TearDown();
    }
};

using FullscreenTest           = LeavesFullscreen<PureGlUi::ShellTest>;
using FullscreenRestoreMacTest = LeavesFullscreen<FullscreenRestoreTest>;

// Made fullscreen as the green button makes it, once the loop has read the window's first
// size: the state saved, and beside it the window it was, never the screen's size
TEST_F(FullscreenTest, AWindowMadeFullscreenIsSavedSo)
{
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    bool            isEnlarged  = false;
    PureGlUi::runPasses(
    [this, &isEnlarged](int pass) {
        if (pass == 1) {
            isEnlarged = PureGlUi::setEnlarged(mainWindow(), true);
        }
    },
    m_shell);
    ASSERT_TRUE(isEnlarged) << "AppKit gave no fullscreen here";
    EXPECT_TRUE(mainWindow().isFullscreen()) << m_read;
    EXPECT_TRUE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(widthBefore)) << m_read;
}

// Negative: out of it again, saved windowed, the window it was
TEST_F(FullscreenTest, AWindowMadeWindowedAgainIsSavedNotFullscreen)
{
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    bool            isEnlarged  = false;
    bool            isRestored  = false;
    PureGlUi::runPasses(
    [this, &isEnlarged, &isRestored](int pass) {
        if (pass == 1) {
            isEnlarged = PureGlUi::setEnlarged(mainWindow(), true);
        } else if (pass == 4) {
            isRestored = PureGlUi::setEnlarged(mainWindow(), false);
        }
    },
    m_shell);
    ASSERT_TRUE(isEnlarged) << "AppKit gave no fullscreen here";
    ASSERT_TRUE(isRestored) << "AppKit never left fullscreen";
    EXPECT_FALSE(mainWindow().isFullscreen()) << m_read;
    EXPECT_FALSE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(widthBefore)) << m_read;
}

// A session that ended fullscreen starts so: asked of AppKit, whose animation ends it a while on
TEST_F(FullscreenRestoreMacTest, AWindowSavedFullscreenStartsFullscreen)
{
    bool isReached = false;
    PureGlUi::runPasses(
    [this, &isReached](int pass) {
        if (pass == 1) {
            isReached = PureGlUi::waitUntil(mainWindow(), [this] { return mainWindow().isFullscreen(); });
        }
    },
    m_shell);
    EXPECT_TRUE(isReached) << m_read;
    EXPECT_TRUE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
}

#elifdef HAVE_WAYLAND

using FullscreenTest = PureGlUi::ShellTest;

// Asked for once the loop has read the window's first size, which is then the one it was
TEST_F(FullscreenTest, AWindowMadeFullscreenIsSavedSo)
{
    const Ui::fpx_t widthBefore = mainWindow().bound().w;
    PureGlUi::runPasses(
    [this](int pass) {
        if (pass == 1) {
            mainWindow().setFullscreen(true);
        }
    },
    m_shell);
    if (mainWindow().bound().w == widthBefore) {
        GTEST_SKIP() << "the compositor gave no fullscreen here";
    }
    EXPECT_TRUE(mainWindow().isFullscreen()) << m_read;
    EXPECT_TRUE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(widthBefore)) << m_read;
}

// The window it was saved again, never the screen's size
TEST_F(FullscreenTest, AWindowMadeWindowedAgainIsSavedNotFullscreen)
{
    const Ui::fpx_t widthBefore   = mainWindow().bound().w;
    bool            wasFullscreen = false;
    PureGlUi::runPasses(
    [this, &wasFullscreen](int pass) {
        if (pass == 1) {
            mainWindow().setFullscreen(true);
        } else if (pass == 3) {
            wasFullscreen = mainWindow().isFullscreen();
        } else if (pass == 4) {
            mainWindow().setFullscreen(false);
        }
    },
    m_shell);
    if (!wasFullscreen) {
        GTEST_SKIP() << "the compositor gave no fullscreen here";
    }
    EXPECT_FALSE(mainWindow().isFullscreen()) << m_read;
    EXPECT_FALSE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(widthBefore)) << m_read;
}

// A configure fed to the window's own listener, as the compositor would send it
void configure(xdg_toplevel * toplevel, int32_t width, int32_t height, bool isFullscreen)
{
    const auto * listener = static_cast<const xdg_toplevel_listener *>(
    wl_proxy_get_listener(PureGlUi::proxyOf(toplevel)));
    wl_array states {};
    wl_array_init(&states);
    if (isFullscreen) {
        *static_cast<uint32_t *>(wl_array_add(&states, sizeof(uint32_t))) = XDG_TOPLEVEL_STATE_FULLSCREEN;
    }
    listener->configure(xdg_toplevel_get_user_data(toplevel), toplevel, width, height, &states);
    wl_array_release(&states);
}

// A compositor that saw the window windowed sends that size back on leaving fullscreen, so the
// configures come by hand: fullscreen, the output's scale doubled, then 0x0, the size left to
// us. The window it was stands the size on screen it was, twice its px
TEST_F(FullscreenTest, AWindowLeftFullscreenAfterAChangeOfScaleIsTheSizeItWas)
{
    xdg_toplevel *  toplevel    = mainWindow().xdgToplevel();
    const int       scaleBefore = mainWindow().dpi() / 96;
    const Ui::fpx_t widthBefore = mainWindow().bound().w;

    configure(toplevel, 1920, 1080, true);
    const bool wasFullscreen = mainWindow().isFullscreen();
    mainWindow().handleOutputScale(scaleBefore * 2);
    configure(toplevel, 0, 0, false);
    const Ui::fpx_t widthAfter = mainWindow().bound().w;
    mainWindow().handleOutputScale(scaleBefore);

    ASSERT_TRUE(wasFullscreen);
    EXPECT_FALSE(mainWindow().isFullscreen());
    EXPECT_FLOAT_EQ(widthAfter, widthBefore * 2.0F) << "before " << widthBefore << " at scale " << scaleBefore;
}

// Fullscreen given at the size the window already has, as a window as large as its screen gets
// it, by hand since no compositor here does: the state reaches the session all the same
TEST_F(FullscreenTest, AWindowMadeFullscreenAtItsOwnSizeIsSavedSo)
{
    xdg_toplevel * toplevel = mainWindow().xdgToplevel();
    const int      width    = Ui::roundToInt(mainWindow().bound().w / mainWindow().bufferScale());
    const int      height   = Ui::roundToInt(mainWindow().bound().h / mainWindow().bufferScale());
    PureGlUi::runPasses(
    [toplevel, width, height](int pass) {
        if (pass == 1) {
            configure(toplevel, width, height, true);
        }
    },
    m_shell);
    EXPECT_TRUE(mainWindow().isFullscreen()) << m_read;
    EXPECT_TRUE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
}

// Negative: nothing asked, nothing saved
TEST_F(FullscreenTest, AWindowNeverFullscreenIsSavedNotFullscreen)
{
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_FALSE(mainWindow().isFullscreen()) << m_read;
    EXPECT_FALSE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
}

TEST_F(FullscreenRestoreTest, AWindowSavedFullscreenStartsFullscreen)
{
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_TRUE(mainWindow().isFullscreen()) << m_read;
}

// Made windowed again, it is the window the session kept: that size, and kept so
TEST_F(FullscreenRestoreTest, AWindowSavedFullscreenComesBackAtItsSavedSize)
{
    bool wasFullscreen = false;
    PureGlUi::runPasses(
    [this, &wasFullscreen](int pass) {
        if (pass == 3) {
            wasFullscreen = mainWindow().isFullscreen();
        } else if (pass == 4) {
            mainWindow().setFullscreen(false);
        }
    },
    m_shell);
    if (!wasFullscreen) {
        GTEST_SKIP() << "the compositor gave no fullscreen here";
    }
    EXPECT_FALSE(mainWindow().isFullscreen()) << m_read;
    EXPECT_EQ(Ui::roundToInt(mainWindow().bound().w), PureGlUi::RESTORED_WIDTH) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), PureGlUi::RESTORED_WIDTH) << m_read;
}

#else

// Negative: X11 reports no fullscreen state, so a session a native Wayland run ended fullscreen is
// saved windowed again, its geometry the window's - left so, the X11 run would save none
TEST_F(FullscreenRestoreTest, AWindowSavedFullscreenIsSavedWindowedWhereNoneIsReported)
{
    PureGlUi::runPasses([](int /*pass*/) { }, m_shell);
    EXPECT_FALSE(m_shell.resManager().isSessionWindowFullscreen()) << m_read;
    EXPECT_EQ(m_shell.resManager().sessionWindowWidth(), Ui::roundToInt(mainWindow().bound().w)) << m_read;
}

#endif

} // namespace

#endif
