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

#pragma once

#include "ui/color.h"
#include "ui/interface/ieventapp.h"
#include "ui/res/type/bound.h"
#include "ui/window/nativedisplayhandle.h"
#include "ui/window/nativewindowhandle.h"

#include <functional>
#include <string>

namespace Ui {

/**
 * @brief Base window interface
 *
 * Common interface for all window types (native, child, managed).
 * Provides core functionality shared across NativeWindow and ChildWindow.
 */
class IWindow : public Ui::IEventApp {
public:
    ~IWindow() override = default;

    // Pre-creation helpers
    virtual void setPosition(fpx_t x, fpx_t y) = 0;

    // Core window interface
    // Extended create signature: optional title and platform-specific handles (display, parent). Pass nullptr for
    // unused on other platforms.
    virtual bool                                          create(fpx_t                           width,
                                                                 fpx_t                           height,
                                                                 Ui::Window::NativeDisplayHandle display,
                                                                 Ui::Window::NativeWindowHandle  parentWindow,
                                                                 const std::string &             title) = 0;
    [[nodiscard]] virtual bool                            isValid() const                   = 0;
    [[nodiscard]] virtual Ui::Window::NativeWindowHandle  nativeHandle() const              = 0;
    [[nodiscard]] virtual Ui::Window::NativeDisplayHandle nativeDisplay() const             = 0;
    virtual void                                          destroy()                         = 0;

    // Title & screen helpers
    virtual void setTitle(const std::string & title)                = 0;
    virtual void screenPosition(int & screenX, int & screenY) const = 0;

    // Screen position of the window's outer frame (decorated by the WM /
    // OS on platforms that draw a title bar). screenPosition() returns the
    // client area's top-left; that value does NOT round-trip through
    // moveResize() on reparenting WMs - the WM treats moveResize as
    // "place the frame at (x, y)" and the client ends up below by the
    // title-bar height, so save/restore drifts the window down by the
    // frame top on every cycle. Use this method for session persistence;
    // use screenPosition() for popup placement (which is anchored to the
    // client area). Default implementation delegates - platforms without
    // client-server-managed decorations (e.g. Wayland) inherit it.
    virtual void screenFramePosition(int & screenX, int & screenY) const { screenPosition(screenX, screenY); }

    // Resize and reposition
    virtual void resize(fpx_t width, fpx_t height)                = 0;
    virtual void move(fpx_t x, fpx_t y)                           = 0;
    virtual void moveResize(const Ui::Res::Type::bound_t & bound) = 0;

    // Visibility
    virtual void show() = 0;
    virtual void hide() = 0;

    // Appearance
    virtual void setBackground(const Ui::Color & color)        = 0;
    virtual void setRoundedCorners(bool enabled, fpx_t radius) = 0;

    // Geometry
    [[nodiscard]] virtual Ui::Res::Type::bound_t bound() const = 0;

    // True if the GL context backing this window runs on a real GPU.
    // False only on platforms that fell back to a software/CPU rasterizer
    // (e.g. macOS without GPU passthrough). Consumers gate features that
    // misbehave or stall on software GL (IBL prefilter, MSAA, ...).
    [[nodiscard]] virtual bool isHardwareGl() const = 0;

    // Optional platform-specific operations used by render threads
    virtual void makeCurrent() = 0; // Make this window's GL context current
    virtual void swapBuffers() = 0; // Swap the front/back buffers
    virtual void clear()       = 0; // Clear framebuffer with background color

    // Render lifecycle (implemented by WindowBase): makeCurrent + renderer->render,
    // re-present the last frame, and queue a render via the render-request callback.
    virtual bool render()        = 0;
    virtual void refresh()       = 0;
    virtual void requestRender() = 0;

    // Wire the render-request callback (the window layer points this at its render queue).
    virtual void setRenderRequest(std::function<void()> fn) = 0;
};

} // namespace Ui
