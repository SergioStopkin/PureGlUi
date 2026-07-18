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

#include "common/bit.h"
#include "common/bytes.h"
#include "common/cstr.h"
#include "ui/gl/localglew.h"
#include "ui/window/eglcontext.h"
#include "ui/window/platform/x11include.h"
#include "ui/window/windowbase.h"

#include <algorithm>
#include <array>
#include <cairo.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <librsvg/rsvg.h>
#include <memory>
#include <string_view>
#include <vector>

namespace Ui::Window::Platform {

/**
 * @brief X11/EGL window implementation
 *
 * Implements Ui::IWindow for Linux using X11 for window management and EGL for OpenGL context.
 * Main windows create their own EGL context; child windows (for content surfaces) let the content surface manage the
 * context.
 */
class X11Window : public WindowBase<X11Window> {
public:
    explicit X11Window(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : WindowBase(subscribe, subscribeId)
    {
    }
    ~X11Window() override
    {
        // Qualified: in a destructor virtual dispatch stops at this class anyway;
        // spelling it out documents that and keeps derived overrides out of play.
        X11Window::destroy();
    }

    // -------- Ui::IWindow implementation --------

    [[nodiscard]] NativeDisplayHandle nativeDisplay() const override { return m_display; }

    bool create(fpx_t               width,
                fpx_t               height,
                NativeDisplayHandle display,
                NativeWindowHandle  parentWindow,
                const std::string & title) override
    {
        m_bound.w = width;
        m_bound.h = height;

        const bool isChildWindow = (parentWindow != 0);

        // Setup display
        if (!initDisplay(display, isChildWindow)) {
            return false;
        }

        // Get visual info and parent window
        X11VisualParams visParams;
        if (!visualInfo(parentWindow, isChildWindow, visParams)) {
            return false;
        }

        // Create the X11 window
        if (!createWindow(visParams, isChildWindow)) {
            return false;
        }

        // Map and configure window
        XMapWindow(m_display, m_xWindow);

        if (isChildWindow) {
            XFlush(m_display);
            XSync(m_display, X11::False);
            std::cout << "[X11Window] Child window created (the content surface manages EGL context)" << std::endl;
        } else {
            if (!setupMainWindow(title)) {
                return false;
            }
        }

        // Apply rounded corners configuration (if requested)
        applyRoundedCorners();

        if (isChildWindow) {
            return (m_display != nullptr) && (m_xWindow != 0U);
        }
        return (m_display != nullptr) && (m_xWindow != 0U) && m_context && m_context->isValid();
    }

    [[nodiscard]] bool isValid() const override { return m_xWindow != 0U; }

    [[nodiscard]] NativeWindowHandle nativeHandle() const override { return m_xWindow; }

    void updateNativeBackground()
    {
        if (m_display != nullptr && m_xWindow != 0U) {
            XSetWindowBackground(m_display, m_xWindow, m_bgColor.toArgb32());
        }
    }

    void destroy() override
    {
        // Cleanup and release renderer before EGL context (needs valid GL for GPU resource cleanup)
        if (m_context) {
            m_context->cleanup();
            m_context.reset();
        }
        if (m_xWindow != 0U) {
            XDestroyWindow(m_display, m_xWindow);
            m_xWindow = 0;
        }
        if (m_display != nullptr && m_ownsDisplay) {
            XCloseDisplay(m_display);
            m_display = nullptr;
        }
    }

    static int queryDpi(NativeDisplayHandle display)
    {
        if (display == nullptr) {
            std::cout << "[X11Window] DPI fallback (no display): 96" << std::endl;
            return 96;
        }

        // Read RESOURCE_MANAGER fresh from the root window every call.
        // XResourceManagerString() returns the value Xlib cached when the
        // display connection was opened, so later Xft.dpi changes from the DE
        // never make it through.
        const Atom                          rmAtom       = XInternAtom(display, "RESOURCE_MANAGER", X11::False);
        Atom                                actualType   = 0;
        int                                 actualFormat = 0;
        Ui::Window::Platform::X11::Cardinal nitems       = 0;
        Ui::Window::Platform::X11::Cardinal bytesAfter   = 0;
        unsigned char *                     data         = nullptr;
        if (XGetWindowProperty(display,
                               DefaultRootWindow(display),
                               rmAtom,
                               0,
                               65536 / 4, // up to 64 KB of resource string
                               X11::False,
                               XA_STRING,
                               &actualType,
                               &actualFormat,
                               &nitems,
                               &bytesAfter,
                               &data)
            == 0
            && data != nullptr) {
            // Xlib appends a NUL byte after property data, so the text is a C string.
            const std::string_view res = Common::viewCString(data);
            const auto             pos = res.find("Xft.dpi:");
            if (pos != std::string_view::npos) {
                auto val = res.substr(pos + 8);
                val.remove_prefix(std::min(val.find_first_not_of(" \t"), val.size()));
                const int dpiVal = static_cast<int>(std::strtol(val.data(), nullptr, 10));
                if (dpiVal > 0) {
                    std::cout << "[X11Window] DPI from Xft.dpi: " << dpiVal << std::endl;
                    XFree(data);
                    return dpiVal;
                }
            }
            XFree(data);
        }

        const int screen  = DefaultScreen(display);
        const int widthPx = DisplayWidth(display, screen);
        const int widthMm = DisplayWidthMM(display, screen);
        if (widthMm > 0) {
            const int dpiVal = static_cast<int>((widthPx * 25.4) / widthMm);
            if (dpiVal > 0) {
                std::cout << "[X11Window] DPI from physical: " << dpiVal << " (" << widthPx << "px / " << widthMm
                          << "mm)" << std::endl;
                return dpiVal;
            }
        }

        std::cout << "[X11Window] DPI fallback: 96" << std::endl;
        return 96;
    }

    void resize(fpx_t width, fpx_t height) override
    {
        m_bound.w = width;
        m_bound.h = height;
        this->requestRender();
        if (m_display != nullptr && m_xWindow != 0U) {
            XResizeWindow(m_display, m_xWindow, static_cast<int>(width), static_cast<int>(height));
            XFlush(m_display);
            applyRoundedCorners();
        }
    }

    void move(fpx_t x, fpx_t y) override
    {
        m_bound.x = x;
        m_bound.y = y;
        if (m_display != nullptr && m_xWindow != 0U) {
            XMoveWindow(m_display, m_xWindow, static_cast<int>(x), static_cast<int>(y));
            XFlush(m_display);
        }
    }

    void show() override
    {
        if (m_display != nullptr && m_xWindow != 0U) {
            XMapWindow(m_display, m_xWindow);
            XFlush(m_display);
        }
    }

    void hide() override
    {
        if (m_display != nullptr && m_xWindow != 0U) {
            XUnmapWindow(m_display, m_xWindow);
            XFlush(m_display);
        }
    }

    void moveResize(const Ui::Res::Type::bound_t & bound) override
    {
        m_bound = bound;
        this->requestRender();
        if (m_display != nullptr && m_xWindow != 0U) {
            XMoveResizeWindow(m_display, m_xWindow, bound.x, bound.y, bound.w, bound.h);
            // XSync ensures the server has processed the resize before we
            // render into this window (XFlush only sends, does not wait).
            XSync(m_display, X11::False);
            applyRoundedCorners();
        }
    }

    void setTitle(const std::string & title) override
    {
        if (m_display != nullptr && m_xWindow != 0U) {
            XStoreName(m_display, m_xWindow, title.c_str());
            XFlush(m_display);
        }
    }

    void screenPosition(int & screenX, int & screenY) const override
    {
        if (m_display != nullptr && m_xWindow != 0U) {
            const ::Window root = DefaultRootWindow(m_display);
            ::Window       child {};
            int            x {};
            int            y {};
            XTranslateCoordinates(m_display, m_xWindow, root, 0, 0, &x, &y, &child);
            screenX = x;
            screenY = y;
        } else {
            screenX = m_bound.x;
            screenY = m_bound.y;
        }
    }

    // Frame top-left. Reads _NET_FRAME_EXTENTS (left, right, top, bottom)
    // and subtracts left/top from the client position. Reparenting WMs
    // (mutter, KWin, Openbox, ...) place the client below the title bar,
    // and a subsequent XMoveResizeWindow expects frame coordinates - so
    // saving screenPosition and restoring with moveResize drifts the
    // window down by `top` on every restart. The WM only sets this atom
    // for mapped, managed top-level windows; if the property is missing
    // we fall back to client position (no decorations or compositor not
    // ready yet - the only cost is one bad save cycle until the WM
    // populates the property, then it stabilises).
    void screenFramePosition(int & screenX, int & screenY) const override
    {
        screenPosition(screenX, screenY);
        if (m_display == nullptr || m_xWindow == 0U) {
            return;
        }
        const Atom                          frameExtentsAtom = XInternAtom(m_display, "_NET_FRAME_EXTENTS", X11::False);
        Atom                                actualType       = 0;
        int                                 actualFormat     = 0;
        Ui::Window::Platform::X11::Cardinal nitems           = 0;
        Ui::Window::Platform::X11::Cardinal bytesAfter       = 0;
        unsigned char *                     data             = nullptr;
        if (XGetWindowProperty(m_display,
                               m_xWindow,
                               frameExtentsAtom,
                               0,
                               4,
                               X11::False,
                               XA_CARDINAL,
                               &actualType,
                               &actualFormat,
                               &nitems,
                               &bytesAfter,
                               &data)
            == 0
            && data != nullptr && actualFormat == 32 && nitems >= 4) {
            // 32-format property data is an array of long-sized CARDINALs (Xlib
            // contract); memcpy is the defined-behavior way to read it without a
            // pointer cast.
            std::array<Ui::Window::Platform::X11::Cardinal, 4> extents {};
            std::memcpy(extents.data(), data, sizeof(Ui::Window::Platform::X11::Cardinal) * extents.size());
            screenX -= static_cast<int>(extents[0]); // left
            screenY -= static_cast<int>(extents[2]); // top
        }
        if (data != nullptr) {
            XFree(data);
        }
    }

    void makeCurrent() override
    {
        // Only make current if we own an EGL context (main window)
        // Child windows let the content surface manage the context
        if (m_context && m_context->isValid()) {
            m_context->makeCurrent();
        }
    }

    void swapBuffers() override
    {
        // Only swap if we own an EGL context (main window)
        // Child windows let the content surface manage buffer swapping
        if (m_context && m_context->isValid()) {
            m_context->swapBuffers();
        }
    }

    void clear() override
    {
        makeCurrent();
        glClear(Common::Bit::Or(GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT));
    }

    // -------- X11-specific methods (called by WindowBase via CRTP) --------

    void applyRoundedCorners()
    {
        if ((m_display == nullptr) || (m_xWindow == 0U)) {
            return;
        }

        if (!m_rounded || m_cornerRadius <= 0) {
            int evb {};
            int errb {};
            if (XShapeQueryExtension(m_display, &evb, &errb) != 0) {
                // Remove custom mask (restore rectangular window)
                XShapeCombineMask(m_display, m_xWindow, ShapeBounding, 0, 0, X11::None, ShapeSet);
                XFlush(m_display);
            }
            return;
        }

        // X11 shape APIs require int; cast once from fpx_t members
        auto radius = static_cast<int>(m_cornerRadius);
        auto w      = static_cast<int>(m_bound.w);
        auto h      = static_cast<int>(m_bound.h);

        int evb {};
        int errb {};
        if (XShapeQueryExtension(m_display, &evb, &errb) == 0) {
            std::cout << "[X11Window] Shape extension not available; cannot apply rounded corners" << std::endl;
            return;
        }

        // Create 1-bit pixmap to act as mask
        const Pixmap mask = XCreatePixmap(m_display, m_xWindow, std::max(1, w), std::max(1, h), 1);
        GC           gc   = XCreateGC(m_display, mask, 0, nullptr);

        // Clear mask to 0
        XSetForeground(m_display, gc, 0);
        XFillRectangle(m_display, mask, gc, 0, 0, w, h);

        // Set foreground to 1 and draw rounded rectangle using rectangles + arcs
        XSetForeground(m_display, gc, 1);

        // If radius is too large, just fill a rectangle (fallback)
        if (radius * 2 >= w || radius * 2 >= h) {
            XFillRectangle(m_display, mask, gc, 0, 0, w, h);
        } else {
            // Center areas
            XFillRectangle(m_display, mask, gc, radius, 0, w - 2 * radius, h);
            XFillRectangle(m_display, mask, gc, 0, radius, w, h - 2 * radius);

            // Corner arcs (filled quarter-circles)
            XFillArc(m_display, mask, gc, 0, 0, radius * 2, radius * 2, 90 * 64, 90 * 64);               // top-left
            XFillArc(m_display, mask, gc, w - 2 * radius, 0, radius * 2, radius * 2, 0 * 64, 90 * 64);   // top-right
            XFillArc(m_display, mask, gc, 0, h - 2 * radius, radius * 2, radius * 2, 180 * 64, 90 * 64); // bottom-left
            XFillArc(m_display,
                     mask,
                     gc,
                     w - 2 * radius,
                     h - 2 * radius,
                     radius * 2,
                     radius * 2,
                     270 * 64,
                     90 * 64); // bottom-right
        }

        // Apply the mask as the bounding shape of the window
        XShapeCombineMask(m_display, m_xWindow, ShapeBounding, 0, 0, mask, ShapeSet);

        // Cleanup
        XFreeGC(m_display, gc);
        XFreePixmap(m_display, mask);
        XFlush(m_display);
    }

protected:
    // -------- X11-specific members (protected for derived classes like PopupWindow) --------

    Display *                     m_display = nullptr;
    ::Window                      m_xWindow = 0;
    std::unique_ptr<Ui::IContext> m_context;
    bool                          m_ownsDisplay = false;

private:
    // -------- Helper structs --------

    struct alignas(64) X11VisualParams final {
        ::Window      parentWin = 0;
        Colormap      colormap  = 0;
        int           depth     = 0;
        Visual *      visual    = nullptr;
        XVisualInfo * vi        = nullptr; // Only set for main window, must be freed
    };

    // -------- Helper methods --------

    bool initDisplay(NativeDisplayHandle xDisplay, bool isChildWindow)
    {
        if (isChildWindow) {
            m_display     = xDisplay;
            m_ownsDisplay = false;
        } else {
            m_display     = XOpenDisplay(nullptr);
            m_ownsDisplay = true;
        }
        if (m_display == nullptr) {
            // getenv is safe here: single-threaded init and the process never setenv's.
            std::cerr << "[X11Window] XOpenDisplay failed (DISPLAY="
                      // NOLINTNEXTLINE(concurrency-mt-unsafe)
                      << (std::getenv("DISPLAY") != nullptr ? std::getenv("DISPLAY") : "unset") << ")" << std::endl;
            return false;
        }
        return true;
    }

    bool visualInfo(NativeWindowHandle parentWindow, bool isChildWindow, X11VisualParams & params)
    {
        if (isChildWindow) {
            params.parentWin = parentWindow;

            XWindowAttributes parentAttr;
            if (XGetWindowAttributes(m_display, params.parentWin, &parentAttr) == 0) {
                return false;
            }

            params.colormap = parentAttr.colormap;
            params.depth    = parentAttr.depth;
            params.visual   = parentAttr.visual;
            params.vi       = nullptr;
        } else {
            // Main window: initialize EGL and get visual from EGL config
            m_context = std::make_unique<Ui::Window::EglContext>();
            if (!m_context->init(m_display)) {
                return false;
            }
            // Main window owns the X connection, so it terminates the shared
            // EGLDisplay on teardown (popups reuse this connection).
            m_context->setOwnsDisplay(m_ownsDisplay);

            if (!m_context->chooseConfig(true, true)) { // want alpha + MSAA
                return false;
            }

            const uint64_t vid = m_context->visualId();
            XVisualInfo    viTemplate;
            viTemplate.visualid = vid;
            int numVisuals      = 0;
            params.vi           = XGetVisualInfo(m_display, VisualIDMask, &viTemplate, &numVisuals);
            if (params.vi == nullptr || numVisuals == 0) {
                std::cerr << "[X11Window] Failed to get X11 visual" << std::endl;
                return false;
            }

            params.parentWin = DefaultRootWindow(m_display);
            params.colormap  = XCreateColormap(m_display, params.parentWin, params.vi->visual, AllocNone);
            params.depth     = params.vi->depth;
            params.visual    = params.vi->visual;
        }
        return true;
    }

    bool createWindow(X11VisualParams & params, bool isChildWindow)
    {
        XSetWindowAttributes winAttrs;
        winAttrs.background_pixel = m_bgColor.toArgb32();
        winAttrs.border_pixel     = 0;
        winAttrs.colormap         = params.colormap;
        winAttrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
                            | StructureNotifyMask;

        if (!isChildWindow) {
            winAttrs.event_mask = static_cast<int64_t>(Common::Bit::Or(winAttrs.event_mask, LeaveWindowMask));
        }

        m_xWindow = XCreateWindow(m_display,
                                  params.parentWin,
                                  m_bound.x,
                                  m_bound.y,
                                  m_bound.w,
                                  m_bound.h,
                                  0,
                                  params.depth,
                                  InputOutput,
                                  params.visual,
                                  CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
                                  &winAttrs);

        if (params.vi != nullptr) {
            XFree(params.vi);
            params.vi = nullptr;
        }

        if (m_xWindow == 0U) {
            std::cerr << "[X11Window] XCreateWindow failed" << std::endl;
            return false;
        }

        return true;
    }

    bool setupMainWindow(const std::string & title)
    {
        XStoreName(m_display, m_xWindow, title.c_str());

        // Set WM_CLASS for proper application identification
        XClassHint classHint;
        classHint.res_name  = const_cast<char *>("pureglui"); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        classHint.res_class = const_cast<char *>("PureGlUi"); // NOLINT(cppcoreguidelines-pro-type-const-cast)
        XSetClassHint(m_display, m_xWindow, &classHint);

        // Window icon is set by WindowManager after create()

        // Register for WM_DELETE_WINDOW for clean window close
        Atom wmDelete = XInternAtom(m_display, "WM_DELETE_WINDOW", X11::False);
        XSetWMProtocols(m_display, m_xWindow, &wmDelete, 1);

        // Create EGL surface and context
        if (!m_context->createSurface(m_xWindow)) {
            return false;
        }

        if (!m_context->createContext()) {
            return false;
        }

        if (!m_context->makeCurrent()) {
            return false;
        }

        std::cout << "[X11Window] EGL context created successfully" << std::endl;

        // Initialize both back buffers with background color to avoid black flicker during resize
        auto rgba = m_bgColor.toGLRGBA();
        glClearColor(rgba.at(0), rgba.at(1), rgba.at(2), rgba.at(3));
        glClear(Common::Bit::Or(GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT));
        m_context->swapBuffers();
        glClear(Common::Bit::Or(GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT));
        m_context->swapBuffers();

        return true;
    }

    // Helper: render an SVG file at a given size into ARGB pixel data for _NET_WM_ICON
    static bool renderSvgToIconData(RsvgHandle *                                       handle,
                                    int                                                size,
                                    std::vector<Ui::Window::Platform::X11::Cardinal> & iconData)
    {
        cairo_surface_t * surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            return false;
        }

        cairo_t *           cr       = cairo_create(surface);
        const RsvgRectangle viewport = { 0, 0, static_cast<double>(size), static_cast<double>(size) };
        GError *            err      = nullptr;
        if (rsvg_handle_render_document(handle, cr, &viewport, &err) == 0) {
            if (err != nullptr) {
                g_error_free(err);
            }
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return false;
        }

        cairo_surface_flush(surface);

        const uint8_t * data   = cairo_image_surface_get_data(surface);
        const int       stride = cairo_image_surface_get_stride(surface);

        // X11 _NET_WM_ICON format: width, height, then ARGB pixels (as CARDINAL)
        iconData.emplace_back(static_cast<Ui::Window::Platform::X11::Cardinal>(size));
        iconData.emplace_back(static_cast<Ui::Window::Platform::X11::Cardinal>(size));

        for (int row = 0; row < size; ++row) {
            const uint8_t * srcRow = data + row * stride; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            for (int col = 0; col < size; ++col) {
                const int srcOff = col * 4;
                // Cairo ARGB32 on little-endian: B, G, R, A in memory (premultiplied)
                uint8_t       b = srcRow[srcOff + 0]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                uint8_t       g = srcRow[srcOff + 1]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                uint8_t       r = srcRow[srcOff + 2]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                const uint8_t a = srcRow[srcOff + 3]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

                // Unpremultiply for X11 ARGB format
                if (a > 0 && a < 255) {
                    r = static_cast<uint8_t>(std::min(255, (r * 255) / a));
                    g = static_cast<uint8_t>(std::min(255, (g * 255) / a));
                    b = static_cast<uint8_t>(std::min(255, (b * 255) / a));
                }

                iconData.emplace_back(Ui::Color(r, g, b, a).toArgb32());
            }
        }

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return true;
    }

public:
    void setWindowIcon(const std::string & mainIconPath, const std::string & symbolicIconPath) override
    {
        // Load main icon
        GError *     gerr       = nullptr;
        GFile *      gfile      = g_file_new_for_path(mainIconPath.c_str());
        RsvgHandle * mainHandle = rsvg_handle_new_from_gfile_sync(gfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &gerr);
        g_object_unref(gfile);
        if (mainHandle == nullptr) {
            std::cerr << "[X11Window] Failed to load icon: " << mainIconPath;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            return;
        }

        // Try to load symbolic icon (optional)
        RsvgHandle * symbolicHandle = nullptr;
        if (!symbolicIconPath.empty()) {
            GError * serr   = nullptr;
            GFile *  sgfile = g_file_new_for_path(symbolicIconPath.c_str());
            symbolicHandle  = rsvg_handle_new_from_gfile_sync(sgfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &serr);
            g_object_unref(sgfile);
            if (serr != nullptr) {
                g_error_free(serr);
            }
        }

        // NOLINTNEXTLINE(google-runtime-int)
        std::vector<Ui::Window::Platform::X11::Cardinal> iconData;

        // Render symbolic icon for desktop environment contexts (panels, menus, notifications)
        if (symbolicHandle != nullptr) {
            constexpr std::array<int, 6> symbolicSizes = { 8, 16, 22, 24, 32, 48 };
            for (const int size : symbolicSizes) {
                renderSvgToIconData(symbolicHandle, size, iconData);
            }
        }

        // Render main icon for window decorations and large contexts
        constexpr std::array<int, 2> mainSizes = { 512, 1024 };
        for (const int size : mainSizes) {
            renderSvgToIconData(mainHandle, size, iconData);
        }

        if (!iconData.empty()) {
            const Atom netWmIcon = XInternAtom(m_display, "_NET_WM_ICON", X11::False);
            const Atom cardinal  = XInternAtom(m_display, "CARDINAL", X11::False);

            XChangeProperty(m_display,
                            m_xWindow,
                            netWmIcon,
                            cardinal,
                            32,
                            PropModeReplace,
                            Common::asBytes(iconData.data()),
                            iconData.size());

            std::cout << "[X11Window] Window icon set from " << mainIconPath;
            if (symbolicHandle != nullptr) {
                std::cout << " (with symbolic variant for small sizes)";
            }
            std::cout << std::endl;
        }

        // Clean up
        g_object_unref(mainHandle);
        if (symbolicHandle != nullptr) {
            g_object_unref(symbolicHandle);
        }
    }
};

} // namespace Ui::Window::Platform
