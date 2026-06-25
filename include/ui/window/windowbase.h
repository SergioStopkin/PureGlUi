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
#include "ui/gl/localglew.h"
#include "ui/interface/icontext.h"
#include "ui/interface/irenderer.h"
#include "ui/interface/iwindow.h"
#include "ui/pubsub/subscribe.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace Ui::Window {

/**
 * @brief CRTP base class for platform-specific windows
 *
 * Provides common members and implementations shared across all platforms.
 * Derived classes (Platform::X11Window, Platform::Win32Window, Platform::MacOsWindow) implement
 * platform-specific Ui::IWindow methods.
 *
 * @tparam Derived The derived platform-specific window class
 */
template <typename Derived>
class WindowBase : public Ui::IWindow {
public:
    explicit WindowBase(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : m_subscribe(subscribe)
        , m_subscribeId(subscribeId)
    {
    }

    ~WindowBase() override
    {
        if (m_subscribeId != Ui::INVALID_ID) {
            m_subscribe.remove(m_subscribeId);
        }
    }

    // Delete copy constructors (windows are unique resources)
    WindowBase(const WindowBase &)             = delete;
    WindowBase & operator=(const WindowBase &) = delete;

    // -------- Pre-creation helpers --------

    void setPosition(fpx_t x, fpx_t y) override
    {
        m_bound.x = x;
        m_bound.y = y;
    }

    // Set window icon (platform-specific, default no-op)
    virtual void setWindowIcon(const std::string & /* mainIconPath */, const std::string & /* symbolicIconPath */) { }

    // -------- Geometry accessors --------

    [[nodiscard]] Ui::Res::Type::bound_t bound() const override { return m_bound; }

    // Hardware-GL flag. Defaults to true; macOS flips to false when its pixel-format
    // request falls back to the software renderer. Platforms that always run on real
    // GL (X11/Wayland/Win32 paths today) keep the default.
    [[nodiscard]] bool isHardwareGl() const override { return m_isHardwareGl; }

    // -------- Appearance --------

    void setBackground(const Ui::Color & color) override
    {
        m_bgColor = color;

        if (derived().isValid()) {
            derived().makeCurrent();
            auto rgba = m_bgColor.toGLRGBA();
            glClearColor(rgba.at(0), rgba.at(1), rgba.at(2), rgba.at(3));
            derived().updateNativeBackground();
            requestRender();
        }
    }

    // Platform hook: update native window background pixel (no-op by default)
    void updateNativeBackground() { }

    void setRoundedCorners(bool enabled, fpx_t radius) override
    {
        m_rounded      = enabled;
        m_cornerRadius = radius;
        if (derived().isValid()) {
            derived().applyRoundedCorners();
        }
    }

    // -------- Renderer ownership --------

    void setRenderer(std::unique_ptr<Ui::IRenderer> renderer) { m_renderer = std::move(renderer); }
    [[nodiscard]] Ui::IRenderer * renderer() const { return m_renderer.get(); }

    // -------- Ui::IEventApp Interface (forward to renderer, request render on change) --------

    bool onMouseMove(int x, int y) override
    {
        return forwardEvent([&] { return m_renderer->onMouseMove(x, y); });
    }
    bool onMousePress(int x, int y, int clickCount = 1) override
    {
        return forwardEvent([&] { return m_renderer->onMousePress(x, y, clickCount); });
    }
    bool onMouseLeave() override
    {
        return forwardEvent([&] { return m_renderer->onMouseLeave(); });
    }
    bool onScroll(int x, int y, fpx_t deltaY) override
    {
        return forwardEvent([&] { return m_renderer->onScroll(x, y, deltaY); });
    }

    Ui::Render::click_result_t onMouseRelease(int x, int y) override
    {
        if (!m_renderer) {
            return {};
        }
        Ui::Render::click_result_t result = m_renderer->onMouseRelease(x, y);
        if (result.changed) {
            requestRender();
        }
        return result;
    }

    /**
     * @brief Re-present last frame without full re-render (calls renderer->refresh())
     */
    void refresh() override
    {
        if (m_renderer) {
            derived().makeCurrent();
            m_renderer->refresh();
        }
    }

    // -------- Subscribe ownership (RAII: destructor unsubscribes) --------

    void setSubscribeId(id_t subscribeId) { m_subscribeId = subscribeId; }

    [[nodiscard]] id_t                    subscribeId() const { return m_subscribeId; }
    [[nodiscard]] Ui::PubSub::Subscribe & subscribe() { return m_subscribe; }

    // -------- Render request (driven by Subscribe render queue) --------

    using RenderRequestFn = std::function<void()>;

    void setRenderRequest(RenderRequestFn fn) override { m_renderRequest = std::move(fn); }

    void requestRender() override
    {
        if (m_renderRequest) {
            m_renderRequest();
        }
    }

    /**
     * @brief Render the current frame
     *
     * Makes this window's GL context current and calls renderer->render().
     * Called by the render queue -- no dirty check.
     * @return true if frame was rendered
     */
    bool render() override
    {
        if (!m_renderer) {
            return false;
        }

        derived().makeCurrent();
        return m_renderer->render();
    }

    // -------- Dimension sync (for OS resize events) --------

    /**
     * @brief Sync internal dimensions without calling OS resize
     *
     * Use this when responding to OS resize events where the window has
     * already been resized. Avoids feedback loops.
     */
    void syncDimensions(fpx_t width, fpx_t height)
    {
        m_bound.w = width;
        m_bound.h = height;
        requestRender();
    }

private:
    // Forward a bool-returning event to renderer and request render on change
    template <typename Fn>
    bool forwardEvent(Fn && fn)
    {
        if (!m_renderer) {
            return false;
        }
        bool changed = fn();
        if (changed) {
            requestRender();
        }
        return changed;
    }

protected:
    // CRTP helper to access derived class
    Derived &       derived() { return static_cast<Derived &>(*this); }
    const Derived & derived() const { return static_cast<const Derived &>(*this); }

    // -------- Common members --------

    // Subscribe ownership (RAII: destructor calls remove)
    Ui::PubSub::Subscribe & m_subscribe;
    id_t                    m_subscribeId = Ui::INVALID_ID;

    // Renderer ownership
    std::unique_ptr<Ui::IRenderer> m_renderer;

    // Render request callback (set by WindowManager, calls Ui::Window::RenderQueue::request)
    RenderRequestFn m_renderRequest;

    // Geometry
    Ui::Res::Type::bound_t m_bound {};

    // Background color
    Ui::Color m_bgColor {};

    // Rounded corners state
    bool  m_rounded      = false;
    fpx_t m_cornerRadius = 16;

    // True when the GL context runs on a real GPU. Platform create() flips it
    // false on software-renderer fallback (currently only Platform::MacOsWindow).
    bool m_isHardwareGl = true;
};

} // namespace Ui::Window
