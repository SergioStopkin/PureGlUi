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

#include "common/noncopyable.h"
#include "ui/color.h"
#include "ui/gl/localglew.h"
#include "ui/interface/iwindow.h"
#include "ui/pubsub/subscribe.h"
#include "ui/type.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

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
class WindowBase : public Ui::IWindow, private Common::NonCopyable {
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

    // -------- Subscribe ownership (RAII: destructor unsubscribes) --------

    void setSubscribeId(id_t subscribeId) { m_subscribeId = subscribeId; }

    [[nodiscard]] id_t                    subscribeId() const { return m_subscribeId; }
    [[nodiscard]] Ui::PubSub::Subscribe & subscribe() { return m_subscribe; }

    // -------- Render request (driven by Subscribe render queue) --------

    void setRenderRequest(Ui::task_fn_t fn) override { m_renderRequest = std::move(fn); }

    void requestRender() override
    {
        if (m_renderRequest) {
            m_renderRequest();
        }
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

protected:
    // CRTP helper to access derived class
    Derived &       derived() { return static_cast<Derived &>(*this); }
    const Derived & derived() const { return static_cast<const Derived &>(*this); }

    // -------- Common members --------

    // Subscribe ownership (RAII: destructor calls remove)
    Ui::PubSub::Subscribe & m_subscribe;
    id_t                    m_subscribeId = Ui::INVALID_ID;

    // Render request callback (set by WindowManager, calls Ui::Window::RenderQueue::request)
    Ui::task_fn_t m_renderRequest;

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
