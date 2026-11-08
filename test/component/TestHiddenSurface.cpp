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
 * swap never returns, so the frames run on a thread of their own and the test waits for them
 * a while. The framework's EGL context swaps at 0 on Wayland, its loop pacing the frames
 *
 * Native Wayland only, which ./build.sh ct wl builds: X11 has no frame callbacks
 */

#ifdef HAVE_WAYLAND

#include "common/time.h"
#include "shelltest.h"
#include "testconst.h"
#include "ui/pubsub/subscribe.h"
#include "ui/window/nativewindow.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <thread>

namespace {

using HiddenSurfaceTest = PureGlUi::ShellTest;

// One frame, as the render loop draws one
void drawFrame(Ui::Window::NativeWindow & window)
{
    window.makeCurrent();
    window.swapBuffers();
}

// The script on a content-surface-like child, made and closed on a thread of its own, where its
// context is current. A swap that waits for a frame callback that never comes never returns, so
// it ends the run: the teardown would close the display under it
void runOnChild(const Ui::Window::NativeWindow & parent, const std::function<void(Ui::Window::NativeWindow &)> & script)
{
    std::atomic<bool>           isCreated { false };
    std::atomic<bool>           isDone { false };
    std::thread                 frames([&parent, &script, &isCreated, &isDone] {
        {
            Ui::PubSub::Subscribe    subscribe;
            Ui::Window::NativeWindow child(subscribe);
            isCreated = child.create(PureGlUi::CHILD_WIDTH,
                                     PureGlUi::CHILD_HEIGHT,
                                     parent.nativeDisplay(),
                                     parent.nativeHandle(),
                                     "");
            if (isCreated) {
                script(child);
            }
        }
        isDone = true;
    });
    const Common::Time::point_t start = Common::Time::now();
    while (!isDone && Common::Time::now() - start < PureGlUi::FRAMES_MOST) {
        Common::Time::sleepFor(PureGlUi::POLL_INTERVAL);
    }
    if (!isDone) {
        ADD_FAILURE() << "a swap never returned in " << PureGlUi::FRAMES_MOST.count() << " ms";
        std::_Exit(EXIT_FAILURE);
    }
    frames.join();
    EXPECT_TRUE(isCreated) << "no child window here";
}

// A content surface's frame, hidden at once as an inactive workspace's is, then shown and drawn
TEST_F(HiddenSurfaceTest, ASurfaceHiddenRightAfterAFrameDrawsOnceShown)
{
    runOnChild(mainWindow(), [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
        child.show();
        drawFrame(child);
    });
}

// A tab switched away from and back again and again
TEST_F(HiddenSurfaceTest, ASurfaceHiddenAgainAndAgainDrawsEachTimeItIsShown)
{
    runOnChild(mainWindow(), [](Ui::Window::NativeWindow & child) {
        for (int round = 0; round < 3; ++round) {
            drawFrame(child);
            child.hide();
            child.show();
        }
        drawFrame(child);
    });
}

// A frame asked for while hidden swaps nothing, and leaves the one after the show free
TEST_F(HiddenSurfaceTest, AFrameWhileHiddenLeavesTheNextOneFree)
{
    runOnChild(mainWindow(), [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
        drawFrame(child);
        child.show();
        drawFrame(child);
    });
}

// A tab closed while its view is hidden right after a frame
TEST_F(HiddenSurfaceTest, ASurfaceHiddenRightAfterAFrameCloses)
{
    runOnChild(mainWindow(), [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        child.hide();
    });
}

// Negative: a surface never hidden draws frame after frame, so a wait above is the hidden one's
TEST_F(HiddenSurfaceTest, ASurfaceNeverHiddenDrawsFrameAfterFrame)
{
    runOnChild(mainWindow(), [](Ui::Window::NativeWindow & child) {
        drawFrame(child);
        drawFrame(child);
        drawFrame(child);
    });
}

} // namespace

#endif
