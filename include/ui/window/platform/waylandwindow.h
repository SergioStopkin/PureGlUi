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

#ifdef HAVE_WAYLAND

#include "ui/config.h"
#include "ui/gl/localglew.h"
#include "ui/window/eglcontext.h"
#include "ui/window/windowbase.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xdg-shell-client-protocol.h>

namespace Ui::Window::Platform {

/**
 * @brief Wayland/EGL window implementation
 *
 * Implements Ui::IWindow for Linux using Wayland for window management and EGL for OpenGL context.
 * Main windows create their own EGL context; child windows use wl_subsurface.
 */
class WaylandWindow : public WindowBase<WaylandWindow> {
public:
    explicit WaylandWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : WindowBase(subscribe, subscribeId)
    {
    }
    ~WaylandWindow() override
    {
        // Renderer cleanup needs EGL context alive - must happen before destroy()
        if (renderer()) {
            renderer()->cleanup();
        }
        destroy();
    }

    WaylandWindow(const WaylandWindow &)             = delete;
    WaylandWindow(WaylandWindow &&)                  = delete;
    WaylandWindow & operator=(const WaylandWindow &) = delete;
    WaylandWindow & operator=(WaylandWindow &&)      = delete;

    // -------- Ui::IWindow implementation --------

    NativeDisplayHandle nativeDisplay() const override { return m_display; }

    bool create(fpx_t               width,
                fpx_t               height,
                NativeDisplayHandle display      = nullptr,
                NativeWindowHandle  parentWindow = nullptr,
                const std::string & title        = "PureGlUi") override
    {
        m_bound.w = width;
        m_bound.h = height;

        const bool isChildWindow = (parentWindow != nullptr);

        // Setup display
        if (!initDisplay(display, isChildWindow)) {
            return false;
        }

        // Get registry and bind globals
        if (!initRegistry()) {
            return false;
        }

        // Create surface
        if (!createSurface()) {
            return false;
        }

        if (isChildWindow) {
            // Create subsurface for child windows (e.g., content surfaces)
            if (!createSubsurface(parentWindow)) {
                return false;
            }

            // Setup EGL context on the subsurface so the content surface can render into it.
            // content surface has no native handle and relies on
            // the application-managed EGL context being current.
            if (!setupEglContext()) {
                return false;
            }
            std::cout << "[WaylandWindow] Child subsurface created with EGL context" << std::endl;
        } else {
            // Create xdg surface for main window
            if (!createXdgSurface(title)) {
                return false;
            }

            // Setup EGL context
            if (!setupEglContext()) {
                return false;
            }
        }

        wl_display_roundtrip(m_display);

        return m_display && m_surface && m_context && m_context->isValid();
    }

    bool isValid() const override { return m_surface != nullptr; }

    NativeWindowHandle nativeHandle() const override { return m_surface; }

    void destroy() override
    {
        if (m_context) {
            m_context->cleanup();
            m_context.reset();
        }
        if (m_xdgToplevel) {
            xdg_toplevel_destroy(m_xdgToplevel);
            m_xdgToplevel = nullptr;
        }
        if (m_xdgSurface) {
            xdg_surface_destroy(m_xdgSurface);
            m_xdgSurface = nullptr;
        }
        if (m_subsurface) {
            wl_subsurface_destroy(m_subsurface);
            m_subsurface = nullptr;
        }
        if (m_surface) {
            wl_surface_destroy(m_surface);
            m_surface = nullptr;
        }
        if (m_display && m_ownsDisplay) {
            wl_display_disconnect(m_display);
            m_display = nullptr;
        }
    }

    static int queryDpi(NativeDisplayHandle /*nativeDisplay*/)
    {
        const int dpi = m_outputScale * 96;
        std::cout << "[WaylandWindow] DPI from wl_output scale: " << dpi << " (" << m_outputScale << "x)" << std::endl;
        return dpi;
    }

    void resize(fpx_t width, fpx_t height) override
    {
        m_bound.w = width;
        m_bound.h = height;
        this->requestRender();

        if (m_context) {
            auto * eglCtx = static_cast<Ui::Window::EglContext *>(m_context.get());
            eglCtx->resizeWaylandWindow(width, height);
        }
        wl_surface_commit(m_surface);
    }

    void move(fpx_t x, fpx_t y) override
    {
        // Wayland doesn't allow client-side window positioning for security
        // Store for relative positioning of subsurfaces
        m_bound.x = x;
        m_bound.y = y;

        if (m_subsurface) {
            // Subsurface position is in parent's surface-local (logical) coords
            wl_subsurface_set_position(m_subsurface,
                                       static_cast<int>(x / g_config.scale),
                                       static_cast<int>(y / g_config.scale));
            wl_surface_commit(m_surface);
        }
    }

    void show() override
    {
        // TODO(sergio): Wayland show
    }

    void hide() override
    {
        // TODO(sergio): Wayland hide
    }

    void moveResize(const Ui::Res::Type::bound_t & bound) override
    {
        move(bound.x, bound.y);
        resize(bound.w, bound.h);
    }

    void setTitle(const std::string & title) override
    {
        if (m_xdgToplevel) {
            xdg_toplevel_set_title(m_xdgToplevel, title.c_str());
            xdg_toplevel_set_app_id(m_xdgToplevel, "pureglui");
            wl_surface_commit(m_surface);
        }
    }

    void screenPosition(int & screenX, int & screenY) const override
    {
        // Wayland doesn't expose absolute screen position to clients
        // Return stored position (for subsurfaces this is relative to parent)
        screenX = m_bound.x;
        screenY = m_bound.y;
    }

    void makeCurrent() override
    {
        if (m_context && m_context->isValid()) {
            m_context->makeCurrent();
        }
    }

    void swapBuffers() override
    {
        if (m_context && m_context->isValid()) {
            m_context->swapBuffers();
        }
    }

    void clear() override
    {
        makeCurrent();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    // -------- Wayland-specific methods (called by WindowBase via CRTP) --------

    void applyRoundedCorners()
    {
        // Wayland with alpha compositing handles transparency natively
        // No shape mask needed - just ensure alpha channel is in EGL config
    }

    // -------- Wayland event callbacks --------

    void handleConfigure(int32_t width, int32_t height)
    {
        // width/height of 0 means the compositor lets us choose
        if (width <= 0 || height <= 0) {
            return;
        }

        // Configure sends logical (surface) coordinates; convert to physical
        const int physW = width * m_scale;
        const int physH = height * m_scale;

        if (physW != static_cast<int>(m_bound.w) || physH != static_cast<int>(m_bound.h)) {
            m_bound.w = physW;
            m_bound.h = physH;
            this->requestRender();

            if (m_context) {
                auto * eglCtx = static_cast<Ui::Window::EglContext *>(m_context.get());
                eglCtx->resizeWaylandWindow(physW, physH);
            }

            std::cout << "[WaylandWindow] Configure: " << width << "x" << height << " (physical: " << physW << "x"
                      << physH << ")" << std::endl;
        }
    }

    void handleClose() { m_shouldClose = true; }

    void handleOutputScale(int32_t scale)
    {
        if (scale > 0) {
            m_scale       = scale;
            m_outputScale = scale;
            // Tell compositor our buffer is already rendered at this scale
            if (m_surface) {
                wl_surface_set_buffer_scale(m_surface, scale);
            }
        }
    }

    bool shouldClose() const { return m_shouldClose; }

    // Accessors for event handling
    wl_display *    wlDisplay() const { return m_display; }
    wl_surface *    wlSurface() const { return m_surface; }
    xdg_wm_base *   xdgWmBase() const { return m_xdgWmBase; }
    wl_seat *       wlSeat() const { return m_seat; }
    wl_compositor * wlCompositor() const { return m_compositor; }
    wl_shm *        wlShm() const { return m_shm; }
    xdg_surface *   xdgSurface() const { return m_xdgSurface; }

protected:
    wl_display *                  m_display = nullptr;
    wl_surface *                  m_surface = nullptr;
    std::unique_ptr<Ui::IContext> m_context;
    bool                          m_ownsDisplay = false;

private:
    // Wayland globals
    wl_registry *      m_registry      = nullptr;
    wl_compositor *    m_compositor    = nullptr;
    xdg_wm_base *      m_xdgWmBase     = nullptr;
    wl_seat *          m_seat          = nullptr;
    wl_output *        m_output        = nullptr;
    wl_subcompositor * m_subcompositor = nullptr;
    wl_shm *           m_shm           = nullptr;

    // Window surfaces
    xdg_surface *   m_xdgSurface  = nullptr;
    xdg_toplevel *  m_xdgToplevel = nullptr;
    wl_subsurface * m_subsurface  = nullptr;

    // State
    int  m_scale       = 1;
    bool m_shouldClose = false;
    bool m_configured  = false;

    // Output scale shared across instances for queryDpi() (static method needs static state)
    static inline int m_outputScale = 1;

    // -------- Static listeners --------

    static void
    registryGlobal(void * data, wl_registry * registry, uint32_t name, const char * interface, uint32_t version)
    {
        auto * self = static_cast<WaylandWindow *>(data);
        self->handleRegistryGlobal(registry, name, interface, version);
    }

    static void registryGlobalRemove(void * /*data*/, wl_registry * /*registry*/, uint32_t /*name*/)
    {
        // Handle output removal if needed
    }

    static constexpr wl_registry_listener s_registryListener = { registryGlobal, registryGlobalRemove };

    static void xdgWmBasePing(void * /*data*/, xdg_wm_base * wmBase, uint32_t serial)
    {
        xdg_wm_base_pong(wmBase, serial);
    }

    static constexpr xdg_wm_base_listener s_xdgWmBaseListener = { xdgWmBasePing };

    static void xdgSurfaceConfigure(void * data, xdg_surface * surface, uint32_t serial)
    {
        auto * self = static_cast<WaylandWindow *>(data);
        xdg_surface_ack_configure(surface, serial);
        self->m_configured = true;
        wl_surface_commit(self->m_surface);
    }

    static constexpr xdg_surface_listener s_xdgSurfaceListener = { xdgSurfaceConfigure };

    static void
    xdgToplevelConfigure(void * data, xdg_toplevel * /*toplevel*/, int32_t width, int32_t height, wl_array * /*states*/)
    {
        auto * self = static_cast<WaylandWindow *>(data);
        self->handleConfigure(width, height);
    }

    static void xdgToplevelClose(void * data, xdg_toplevel * /*toplevel*/)
    {
        auto * self = static_cast<WaylandWindow *>(data);
        self->handleClose();
    }

    static void
    xdgToplevelConfigureBounds(void * /*data*/, xdg_toplevel * /*toplevel*/, int32_t /*width*/, int32_t /*height*/)
    {
        // Optional: handle suggested maximum size
    }

    static void xdgToplevelWmCapabilities(void * /*data*/, xdg_toplevel * /*toplevel*/, wl_array * /*capabilities*/)
    {
        // Optional: handle window manager capabilities
    }

    static constexpr xdg_toplevel_listener s_xdgToplevelListener = { xdgToplevelConfigure,
                                                                     xdgToplevelClose,
                                                                     xdgToplevelConfigureBounds,
                                                                     xdgToplevelWmCapabilities };

    static void outputGeometry(void * /*data*/,
                               wl_output * /*output*/,
                               int32_t /*x*/,
                               int32_t /*y*/,
                               int32_t /*physWidth*/,
                               int32_t /*physHeight*/,
                               int32_t /*subpixel*/,
                               const char * /*make*/,
                               const char * /*model*/,
                               int32_t /*transform*/)
    {
    }

    static void outputMode(void * /*data*/,
                           wl_output * /*output*/,
                           uint32_t /*flags*/,
                           int32_t /*width*/,
                           int32_t /*height*/,
                           int32_t /*refresh*/)
    {
    }

    static void outputDone(void * /*data*/, wl_output * /*output*/) { }

    static void outputScale(void * data, wl_output * /*output*/, int32_t factor)
    {
        auto * self = static_cast<WaylandWindow *>(data);
        self->handleOutputScale(factor);
    }

    static void outputName(void * /*data*/, wl_output * /*output*/, const char * /*name*/) { }

    static void outputDescription(void * /*data*/, wl_output * /*output*/, const char * /*description*/) { }

    static constexpr wl_output_listener s_outputListener = { outputGeometry, outputMode, outputDone,
                                                             outputScale,    outputName, outputDescription };

    // -------- Helper methods --------

    void handleRegistryGlobal(wl_registry * registry, uint32_t name, std::string_view interface, uint32_t version)
    {
        if (interface == wl_compositor_interface.name) {
            m_compositor = static_cast<wl_compositor *>(
            wl_registry_bind(registry, name, &wl_compositor_interface, std::min(version, 4U)));
        } else if (interface == xdg_wm_base_interface.name) {
            m_xdgWmBase = static_cast<xdg_wm_base *>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 3U)));
            xdg_wm_base_add_listener(m_xdgWmBase, &s_xdgWmBaseListener, this);
        } else if (interface == wl_seat_interface.name) {
            m_seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5U)));
        } else if (interface == wl_output_interface.name) {
            m_output = static_cast<wl_output *>(
            wl_registry_bind(registry, name, &wl_output_interface, std::min(version, 4U)));
            wl_output_add_listener(m_output, &s_outputListener, this);
        } else if (interface == wl_subcompositor_interface.name) {
            m_subcompositor = static_cast<wl_subcompositor *>(
            wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
        } else if (interface == wl_shm_interface.name) {
            m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
        }
    }

    bool initDisplay(NativeDisplayHandle display, bool isChildWindow)
    {
        if (isChildWindow && display) {
            m_display     = display;
            m_ownsDisplay = false;
        } else {
            m_display     = wl_display_connect(nullptr);
            m_ownsDisplay = true;
        }

        if (!m_display) {
            std::cerr << "[WaylandWindow] Failed to connect to Wayland display" << std::endl;
            return false;
        }

        std::cout << "[WaylandWindow] Connected to Wayland display" << std::endl;
        return true;
    }

    bool initRegistry()
    {
        m_registry = wl_display_get_registry(m_display);
        if (!m_registry) {
            std::cerr << "[WaylandWindow] Failed to get registry" << std::endl;
            return false;
        }

        wl_registry_add_listener(m_registry, &s_registryListener, this);
        wl_display_roundtrip(m_display);

        if (!m_compositor) {
            std::cerr << "[WaylandWindow] No compositor found" << std::endl;
            return false;
        }

        if (!m_xdgWmBase) {
            std::cerr << "[WaylandWindow] No xdg_wm_base found" << std::endl;
            return false;
        }

        return true;
    }

    bool createSurface()
    {
        m_surface = wl_compositor_create_surface(m_compositor);
        if (!m_surface) {
            std::cerr << "[WaylandWindow] Failed to create surface" << std::endl;
            return false;
        }
        return true;
    }

    bool createSubsurface(wl_surface * parentSurface)
    {
        if (!m_subcompositor) {
            std::cerr << "[WaylandWindow] No subcompositor available" << std::endl;
            return false;
        }

        m_subsurface = wl_subcompositor_get_subsurface(m_subcompositor, m_surface, parentSurface);
        if (!m_subsurface) {
            std::cerr << "[WaylandWindow] Failed to create subsurface" << std::endl;
            return false;
        }

        wl_subsurface_set_desync(m_subsurface);
        // Subsurface position is in parent's surface-local (logical) coordinates
        wl_subsurface_set_position(m_subsurface,
                                   static_cast<int>(m_bound.x / g_config.scale),
                                   static_cast<int>(m_bound.y / g_config.scale));
        // Tell compositor our buffer is pre-scaled
        wl_surface_set_buffer_scale(m_surface, static_cast<int>(g_config.scale));
        return true;
    }

    bool createXdgSurface(const std::string & title)
    {
        m_xdgSurface = xdg_wm_base_get_xdg_surface(m_xdgWmBase, m_surface);
        if (!m_xdgSurface) {
            std::cerr << "[WaylandWindow] Failed to create xdg_surface" << std::endl;
            return false;
        }
        xdg_surface_add_listener(m_xdgSurface, &s_xdgSurfaceListener, this);

        m_xdgToplevel = xdg_surface_get_toplevel(m_xdgSurface);
        if (!m_xdgToplevel) {
            std::cerr << "[WaylandWindow] Failed to create xdg_toplevel" << std::endl;
            return false;
        }
        xdg_toplevel_add_listener(m_xdgToplevel, &s_xdgToplevelListener, this);

        xdg_toplevel_set_title(m_xdgToplevel, title.c_str());
        xdg_toplevel_set_app_id(m_xdgToplevel, "pureglui");

        // Set min size to hint the compositor we don't want to be too small
        xdg_toplevel_set_min_size(m_xdgToplevel, 640, 480);

        // Commit to trigger initial configure
        wl_surface_commit(m_surface);

        // Wait for initial configure event
        while (!m_configured) {
            wl_display_roundtrip(m_display);
        }

        std::cout << "[WaylandWindow] Initial size after configure: " << m_bound.w << "x" << m_bound.h << std::endl;

        return true;
    }

    bool setupEglContext()
    {
        m_context = Ui::Window::EglContext::create(m_display, Ui::Window::EglPlatform::Wayland);
        if (!m_context) {
            std::cerr << "[WaylandWindow] Failed to init EGL" << std::endl;
            return false;
        }

        if (!m_context->chooseConfig(true, true)) { // want alpha + MSAA
            std::cerr << "[WaylandWindow] Failed to choose EGL config" << std::endl;
            return false;
        }

        auto * eglCtx = static_cast<Ui::Window::EglContext *>(m_context.get());
        if (!eglCtx->createSurface(m_surface, m_bound.w, m_bound.h)) {
            std::cerr << "[WaylandWindow] Failed to create EGL surface" << std::endl;
            return false;
        }

        if (!m_context->createContext()) {
            std::cerr << "[WaylandWindow] Failed to create EGL context" << std::endl;
            return false;
        }

        if (!m_context->makeCurrent()) {
            std::cerr << "[WaylandWindow] Failed to make EGL context current" << std::endl;
            return false;
        }

        std::cout << "[WaylandWindow] EGL context created successfully" << std::endl;

        // Initialize both back buffers with background color
        auto rgba = m_bgColor.toGLRGBA();
        glClearColor(rgba.at(0), rgba.at(1), rgba.at(2), rgba.at(3));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_context->swapBuffers();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        m_context->swapBuffers();

        // Final commit to show the window
        wl_surface_commit(m_surface);
        wl_display_flush(m_display);

        return true;
    }
};

} // namespace Ui::Window::Platform

#endif // HAVE_WAYLAND
