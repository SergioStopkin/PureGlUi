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
 * @file TestContentSurface.cpp
 * @brief WindowManager content-surface registry, headless. Drives the host-facing
 *        add/remove/setActive/setReady seam with a stub IWindow/IRenderer and
 *        asserts the state machine + the centralized childIdForHandle lookup
 *        (the single source event peers resolve native-handle -> child id through).
 *
 * X11-only: NativeWindowHandle is an integer (::Window) here, so distinct handles
 * are trivially expressible; the registry logic is platform-identical. No real
 * window/GL - initialize() is never called, childIdForHandle() is invoked directly.
 */

#if defined(HAVE_X11)

#include "ui/interface/irenderer.h"
#include "ui/interface/iwindow.h"
#include "ui/res/resmanager.h"
#include "ui/window/windowmanager.h"

#include <gtest/gtest.h>
#include <string>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace PureGlUi {

// Pure-surface IWindow stub with a settable native handle - enough to register
// as a content surface and be resolved by handle.
class FakeWindow final : public Ui::IWindow {
public:
    explicit FakeWindow(Ui::Window::NativeWindowHandle handle)
        : m_handle(handle)
    {
    }

    void setPosition(Ui::fpx_t /*x*/, Ui::fpx_t /*y*/) override { }
    bool create(Ui::fpx_t /*width*/,
                Ui::fpx_t /*height*/,
                Ui::Window::NativeDisplayHandle /*display*/,
                Ui::Window::NativeWindowHandle /*parent*/,
                const std::string & /*title*/) override
    {
        return true;
    }
    [[nodiscard]] bool                            isValid() const override { return true; }
    [[nodiscard]] Ui::Window::NativeWindowHandle  nativeHandle() const override { return m_handle; }
    [[nodiscard]] Ui::Window::NativeDisplayHandle nativeDisplay() const override { return nullptr; }
    void                                          destroy() override { }
    void                                          setTitle(const std::string & /*title*/) override { }
    void                                          screenPosition(int & screenX, int & screenY) const override
    {
        screenX = 0;
        screenY = 0;
    }
    void                                 resize(Ui::fpx_t /*width*/, Ui::fpx_t /*height*/) override { }
    void                                 move(Ui::fpx_t /*x*/, Ui::fpx_t /*y*/) override { }
    void                                 moveResize(const Ui::Res::Type::bound_t & /*bound*/) override { }
    void                                 show() override { }
    void                                 hide() override { }
    void                                 setBackground(const Ui::Color & /*color*/) override { }
    void                                 setRoundedCorners(bool /*enabled*/, Ui::fpx_t /*radius*/) override { }
    [[nodiscard]] Ui::Res::Type::bound_t bound() const override { return {}; }
    [[nodiscard]] bool                   isHardwareGl() const override { return false; }
    void                                 makeCurrent() override { }
    void                                 swapBuffers() override { }
    void                                 clear() override { }
    void                                 requestRender() override { }
    void                                 setRenderRequest(Ui::task_fn_t /*fn*/) override { }

private:
    Ui::Window::NativeWindowHandle m_handle;
};

// IRenderer stub - never rendered here, just fills the pairing slot.
class FakeRenderer final : public Ui::IRenderer {
public:
    bool                       render() override { return true; }
    void                       resize(Ui::fpx_t /*width*/, Ui::fpx_t /*height*/) override { }
    void                       apply(Ui::Res::Type::Changed /*changed*/) override { }
    void                       cleanup() override { }
    bool                       onMouseMove(int /*x*/, int /*y*/) override { return false; }
    bool                       onMousePress(int /*x*/, int /*y*/, int /*clickCount*/) override { return false; }
    Ui::Render::click_result_t onMouseRelease(int /*x*/, int /*y*/) override { return {}; }
    bool                       onMouseLeave() override { return false; }
    bool                       onScroll(int /*x*/, int /*y*/, Ui::fpx_t /*deltaY*/) override { return false; }
};

class ContentSurfaceTest : public ::testing::Test {
protected:
    Ui::Res::ResManager       resManager { TEST_RES_DIR };
    Ui::Window::WindowManager windowManager { resManager };
};

// A registered surface's native handle resolves to its id; an unregistered handle
// (e.g. the main window) resolves to INVALID_ID. childIdForHandle is the single
// source the event peers' child-window lookup is wired to.
TEST_F(ContentSurfaceTest, RegisteredHandleResolvesToId)
{
    FakeWindow   windowA { 1001 };
    FakeWindow   windowB { 1002 };
    FakeRenderer rendererA;
    FakeRenderer rendererB;

    windowManager.addContentSurface(10, windowA, rendererA);
    windowManager.addContentSurface(20, windowB, rendererB);

    EXPECT_EQ(windowManager.childIdForHandle(1001), 10);
    EXPECT_EQ(windowManager.childIdForHandle(1002), 20);
    EXPECT_EQ(windowManager.childIdForHandle(9999), Ui::INVALID_ID); // unregistered
    EXPECT_EQ(windowManager.contentWindow(10), &windowA);
    EXPECT_EQ(windowManager.contentPairing(20), &rendererB);
}

// Removing a surface drops it from the lookup and clears the active id when it was
// the active one.
TEST_F(ContentSurfaceTest, RemoveDropsLookupAndResetsActive)
{
    FakeWindow   window { 1001 };
    FakeRenderer renderer;
    windowManager.addContentSurface(10, window, renderer);
    windowManager.setActiveContentSurface(10);
    EXPECT_EQ(windowManager.activeContentWindow(), &window);

    windowManager.removeContentSurface(10);
    EXPECT_EQ(windowManager.childIdForHandle(1001), Ui::INVALID_ID);
    EXPECT_EQ(windowManager.activeContentWindow(), nullptr); // active was 10 -> reset
}

// The ready flag defaults true, toggles, and reads false for an unknown id.
TEST_F(ContentSurfaceTest, ReadyFlagTracks)
{
    FakeWindow   window { 1001 };
    FakeRenderer renderer;
    windowManager.addContentSurface(10, window, renderer);

    EXPECT_TRUE(windowManager.isContentReady(10));
    windowManager.setContentSurfaceReady(10, false);
    EXPECT_FALSE(windowManager.isContentReady(10));
    windowManager.setContentSurfaceReady(10, true);
    EXPECT_TRUE(windowManager.isContentReady(10));
    EXPECT_FALSE(windowManager.isContentReady(999)); // unknown id
}

} // namespace PureGlUi

#endif // HAVE_X11
