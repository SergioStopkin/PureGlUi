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
 * @file TestHiddenSurface.cpp
 * @brief A surface hidden right after a frame draws again once shown, on native Wayland
 *
 * At swap interval 1 Mesa waits, before each swap, for the frame callback the swap before it
 * asked for, and a compositor may never fire one for a surface unmapped before it did: such a
 * swap never returns, so the frames run on a thread of their own, waited for a while, in a child
 * process the wait takes down alone. The framework's EGL context swaps at 0 on Wayland, its loop
 * pacing the frames
 *
 * Native Wayland only, which ./build.sh ct wl builds: X11 has no frame callbacks
 */

#ifdef HAVE_WAYLAND

#include "runwithin.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/pubsub/subscribe.h"
#include "ui/window/nativewindow.h"

#include <atomic>
#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <iostream>

namespace {

// Not named *DeathTest, which gtest runs before every other suite: CMake links this file last,
// so it runs after them
class HiddenSurfaceTest : public PureGlUi::ShellTest {
protected:
    void SetUp() override
    {
        PureGlUi::ShellTest::SetUp();
        // A child forked off this process would draw over its Wayland connection; one run afresh
        // opens its own
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

// One frame, as the render loop draws one
void drawFrame(Ui::Window::NativeWindow & window)
{
    window.makeCurrent();
    window.swapBuffers();
}

// The script on a content-surface-like child, made and closed on a thread of its own, where its
// context is current; then the end of the child process this runs in, its exit saying how it went
[[noreturn]] void runOnChild(const Ui::Window::NativeWindow &                        parent,
                             const std::function<void(Ui::Window::NativeWindow &)> & script)
{
    std::atomic<bool> isCreated { false };
    PureGlUi::runWithin([&parent, &script, &isCreated] {
        Ui::PubSub::Subscribe    subscribe;
        Ui::Window::NativeWindow child(subscribe);
        isCreated =
        child.create(PureGlUi::CHILD_WIDTH, PureGlUi::CHILD_HEIGHT, parent.nativeDisplay(), parent.nativeHandle(), "");
        if (isCreated) {
            script(child);
        }
    });
    if (!isCreated) {
        std::cerr << "no child window here" << std::endl;
        std::_Exit(EXIT_FAILURE);
    }
    std::_Exit(EXIT_SUCCESS);
}

// A content surface's frame, hidden at once as an inactive workspace's is, then shown and drawn
TEST_F(HiddenSurfaceTest, ASurfaceHiddenRightAfterAFrameDrawsOnceShown)
{
    const auto script = [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
        child.show();
        drawFrame(child);
    };
    EXPECT_EXIT(runOnChild(mainWindow(), script), testing::ExitedWithCode(EXIT_SUCCESS), "");
}

// A tab switched away from and back again and again
TEST_F(HiddenSurfaceTest, ASurfaceHiddenAgainAndAgainDrawsEachTimeItIsShown)
{
    const auto script = [](Ui::Window::NativeWindow & child) {
        for (int round = 0; round < 3; ++round) {
            drawFrame(child);
            child.hide();
            child.show();
        }
        drawFrame(child);
    };
    EXPECT_EXIT(runOnChild(mainWindow(), script), testing::ExitedWithCode(EXIT_SUCCESS), "");
}

// A frame asked for while hidden swaps nothing, and leaves the one after the show free
TEST_F(HiddenSurfaceTest, AFrameWhileHiddenLeavesTheNextOneFree)
{
    const auto script = [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
        drawFrame(child);
        child.show();
        drawFrame(child);
    };
    EXPECT_EXIT(runOnChild(mainWindow(), script), testing::ExitedWithCode(EXIT_SUCCESS), "");
}

// A tab closed while its view is hidden right after a frame
TEST_F(HiddenSurfaceTest, ASurfaceHiddenRightAfterAFrameCloses)
{
    const auto script = [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
    };
    EXPECT_EXIT(runOnChild(mainWindow(), script), testing::ExitedWithCode(EXIT_SUCCESS), "");
}

// Negative: a surface never hidden draws frame after frame, so a wait above is the hidden one's
TEST_F(HiddenSurfaceTest, ASurfaceNeverHiddenDrawsFrameAfterFrame)
{
    const auto script = [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        drawFrame(child);
        drawFrame(child);
    };
    EXPECT_EXIT(runOnChild(mainWindow(), script), testing::ExitedWithCode(EXIT_SUCCESS), "");
}

} // namespace

#endif
