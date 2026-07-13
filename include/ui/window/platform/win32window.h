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

#include "common/unicode.h"
#include "ui/window/windowbase.h"

// Include cairo/librsvg BEFORE <windows.h> - glib symbols conflict with
// Windows macros (Rectangle, BOOL, etc.), and we need these headers to
// parse in a clean preprocessor environment.
#include <cairo.h>
#include <librsvg/rsvg.h>

// clang-format off
#include <windows.h>
// clang-format on

#include <GL/gl.h>
#include <chrono>
#include <cstring>
#include <iostream>

namespace Ui::Window::Platform {

// Flip to true to time each phase of Win32Window::destroy() - useful when
// close feels laggy and we need to know whether wglDeleteContext or the
// DestroyWindow message dispatch is the culprit.
constexpr bool WIN32_PERF_DEBUG = true;

/**
 * @brief Win32/WGL window implementation
 *
 * Implements Ui::IWindow for Windows using Win32 for window management and WGL for OpenGL context.
 */
class Win32Window : public WindowBase<Win32Window> {
public:
    // User message re-posted from WndProc so pollEvent() can see it
    static constexpr UINT WM_APP_CLOSE = WM_APP + 1;

    explicit Win32Window(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : WindowBase(subscribe, subscribeId)
    {
    }
    ~Win32Window() override
    {
        // Qualified: in a destructor virtual dispatch stops at this class anyway;
        // spelling it out documents that and keeps derived overrides out of play.
        Win32Window::destroy();
    }

    // -------- Ui::IWindow implementation --------

    [[nodiscard]] NativeDisplayHandle nativeDisplay() const override { return nullptr; }

    bool create(fpx_t               width,
                fpx_t               height,
                NativeDisplayHandle display,
                NativeWindowHandle  parentWindow,
                const std::string & title) override
    {
        (void)display;

        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

        m_bound.w = width;
        m_bound.h = height;

        const HINSTANCE hInstance = GetModuleHandle(nullptr);

        if (parentWindow) {
            // Child window (e.g. content surface) - embedded inside parent, no decorations.
            // WS_CLIPSIBLINGS prevents sibling children from painting over this one.
            // CS_OWNDC: the content surface's WGL context lives on this HWND's DC, so it must be a
            // dedicated per-window DC (WGL requirement). Without CS_OWNDC the DC is
            // pulled from a shared pool and can be reshuffled by the system across
            // resize/paint events, which corrupts the swap chain.
            WNDCLASSW wc     = {};
            wc.style         = CS_OWNDC;
            wc.lpfnWndProc   = DefWindowProcW;
            wc.hInstance     = hInstance;
            wc.lpszClassName = L"PureGlUiChildClass";
            RegisterClassW(&wc);

            m_hwnd = CreateWindowExW(0,
                                     L"PureGlUiChildClass",
                                     L"",
                                     WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                     static_cast<int>(m_bound.x),
                                     static_cast<int>(m_bound.y),
                                     static_cast<int>(width),
                                     static_cast<int>(height),
                                     parentWindow,
                                     nullptr,
                                     hInstance,
                                     nullptr);
        } else {
            // Top-level main window.
            // CS_OWNDC: dedicated DC per window (required for stable WGL context).
            // CS_HREDRAW|CS_VREDRAW: repaint on resize so GL viewport stays correct.
            WNDCLASSW wc     = {};
            wc.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc   = wndProc;
            wc.hInstance     = hInstance;
            wc.lpszClassName = L"PureGlUiWindowClass";
            RegisterClassW(&wc);

            const std::wstring wideTitle = Common::Unicode::fromUtf8(title);

            // AdjustWindowRectEx converts client area to total window size (including title bar and borders).
            // Without this, CreateWindowExW treats width/height as total size, shrinking the client area.
            // WS_CLIPCHILDREN: excludes child window areas from parent's paint region so the main
            // window's GL SwapBuffers does not overwrite the content surface's pixels.
            // WS_CLIPSIBLINGS: same protection between sibling popups.
            constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            RECT            rect  = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
            AdjustWindowRectEx(&rect, style, FALSE, 0);

            m_hwnd = CreateWindowExW(0,
                                     L"PureGlUiWindowClass",
                                     wideTitle.c_str(),
                                     style,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     rect.right - rect.left,
                                     rect.bottom - rect.top,
                                     nullptr,
                                     nullptr,
                                     hInstance,
                                     this);
            SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, asUserData(this));
            // Launched from a terminal (Git Bash etc.) the foreground rights
            // stay with the console, so the freshly created window appears
            // BEHIND it. Bring the main window to the front explicitly; when
            // the OS denies foreground stealing this degrades to a taskbar
            // flash. Popups are unaffected (own creation path, and show() uses
            // SW_SHOWNOACTIVATE so menus never steal focus).
            if (m_hwnd != nullptr) {
                SetForegroundWindow(m_hwnd);
                SetFocus(m_hwnd);
            }
        }

        // Child windows (content surface) - the content surface manages its own GL context on the HWND.
        // Only create the HWND; skip pixel format and WGL context setup.
        if (parentWindow) {
            return m_hwnd != nullptr;
        }

        m_hdc                     = GetDC(m_hwnd);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize                 = sizeof(pfd);
        pfd.nVersion              = 1;
        pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType            = PFD_TYPE_RGBA;
        pfd.cColorBits            = 32;
        pfd.cDepthBits            = 24;
        pfd.iLayerType            = PFD_MAIN_PLANE;
        int pf                    = ChoosePixelFormat(m_hdc, &pfd);
        SetPixelFormat(m_hdc, pf, &pfd);
        m_hglrc = wglCreateContext(m_hdc);
        wglMakeCurrent(m_hdc, m_hglrc);

        applyRoundedCorners();

        return m_hwnd && m_hdc && m_hglrc;
    }

    [[nodiscard]] bool isValid() const override { return m_hwnd != nullptr; }

    [[nodiscard]] NativeWindowHandle nativeHandle() const override { return m_hwnd; }

    void destroy() override
    {
        using clock_t       = std::chrono::steady_clock;
        const auto t_start  = clock_t::now();
        auto       ms_since = [&](clock_t::time_point from) {
            return std::chrono::duration_cast<std::chrono::milliseconds>(clock_t::now() - from).count();
        };

        if (m_hglrc) {
            const auto t = clock_t::now();
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(m_hglrc);
            m_hglrc = nullptr;
            if constexpr (WIN32_PERF_DEBUG) {
                std::cout << "[Win32Window] wglDeleteContext (" << ms_since(t) << "ms)" << std::endl;
            }
        }
        if (m_hdc && m_hwnd) {
            ReleaseDC(m_hwnd, m_hdc);
            m_hdc = nullptr;
        }
        if (m_hwnd) {
            const auto t = clock_t::now();
            DestroyWindow(m_hwnd);
            m_hwnd = nullptr;
            if constexpr (WIN32_PERF_DEBUG) {
                std::cout << "[Win32Window] DestroyWindow (" << ms_since(t) << "ms)" << std::endl;
            }
        }
        if (m_hIconBig) {
            DestroyIcon(m_hIconBig);
            m_hIconBig = nullptr;
        }
        if (m_hIconSmall) {
            DestroyIcon(m_hIconSmall);
            m_hIconSmall = nullptr;
        }
        if constexpr (WIN32_PERF_DEBUG) {
            std::cout << "[Win32Window] destroy total (" << ms_since(t_start) << "ms)" << std::endl;
        }
    }

    static int queryDpi(NativeDisplayHandle /*nativeDisplay*/)
    {
        HDC hdc    = GetDC(NULL);
        int dpiVal = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        std::cout << "[Win32Window] DPI: " << dpiVal << std::endl;
        return dpiVal;
    }

    void resize(fpx_t width, fpx_t height) override
    {
        m_bound.w = width;
        m_bound.h = height;
        this->requestRender();
        if (m_hwnd) {
            SetWindowPos(m_hwnd,
                         NULL,
                         0,
                         0,
                         static_cast<int>(width),
                         static_cast<int>(height),
                         SWP_NOMOVE | SWP_NOZORDER);
            applyRoundedCorners();
        }
    }

    void move(fpx_t x, fpx_t y) override
    {
        m_bound.x = x;
        m_bound.y = y;
        if (m_hwnd) {
            SetWindowPos(m_hwnd, NULL, static_cast<int>(x), static_cast<int>(y), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    void show() override
    {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        }
    }

    void hide() override
    {
        if (m_hwnd) {
            ShowWindow(m_hwnd, SW_HIDE);
        }
    }

    void moveResize(const Ui::Res::Type::bound_t & bound) override
    {
        m_bound = bound;
        this->requestRender();
        if (m_hwnd) {
            // bound carries the frame origin (x, y - what screenFramePosition
            // reports) and the CLIENT size (w, h - what WM_SIZE/session store),
            // but SetWindowPos takes the OUTER size. Convert via the window's
            // actual style - the same adjustment create() applies. For
            // undecorated popups (WS_POPUP) and child windows the adjustment
            // is a no-op, so this stays correct for every window kind.
            RECT       rect    = { 0, 0, static_cast<LONG>(bound.w), static_cast<LONG>(bound.h) };
            const auto style   = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_STYLE));
            const auto exStyle = static_cast<DWORD>(GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
            SetWindowPos(m_hwnd,
                         NULL,
                         static_cast<int>(bound.x),
                         static_cast<int>(bound.y),
                         rect.right - rect.left,
                         rect.bottom - rect.top,
                         SWP_NOZORDER);
            applyRoundedCorners();
        }
    }

    void setTitle(const std::string & title) override
    {
        if (m_hwnd) {
            SetWindowTextW(m_hwnd, Common::Unicode::fromUtf8(title).c_str());
        }
    }

    void screenPosition(int & screenX, int & screenY) const override
    {
        if (m_hwnd) {
            POINT pt {};
            ClientToScreen(m_hwnd, &pt);
            screenX = pt.x;
            screenY = pt.y;
        } else {
            screenX = static_cast<int>(m_bound.x);
            screenY = static_cast<int>(m_bound.y);
        }
    }

    // Frame (outer) top-left for session persistence. moveResize positions the
    // FRAME, so persisting the client origin (screenPosition, above) would walk
    // the window right/down by the border + title-bar on every restart.
    void screenFramePosition(int & screenX, int & screenY) const override
    {
        if (m_hwnd) {
            RECT rect {};
            GetWindowRect(m_hwnd, &rect);
            screenX = rect.left;
            screenY = rect.top;
        } else {
            screenX = static_cast<int>(m_bound.x);
            screenY = static_cast<int>(m_bound.y);
        }
    }

    void makeCurrent() override { wglMakeCurrent(m_hdc, m_hglrc); }

    void swapBuffers() override { SwapBuffers(m_hdc); }

    void clear() override
    {
        makeCurrent();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void setWindowIcon(const std::string & mainIconPath, const std::string & symbolicIconPath) override
    {
        (void)symbolicIconPath; // Win32 uses one icon; OS scales between small/big requests
        if (m_hwnd == nullptr) {
            return;
        }

        GError *     gerr   = nullptr;
        GFile *      gfile  = g_file_new_for_path(mainIconPath.c_str());
        RsvgHandle * handle = rsvg_handle_new_from_gfile_sync(gfile, RSVG_HANDLE_FLAGS_NONE, nullptr, &gerr);
        g_object_unref(gfile);
        if (handle == nullptr) {
            std::cerr << "[Win32Window] Failed to load icon: " << mainIconPath;
            if (gerr != nullptr) {
                std::cerr << " (" << gerr->message << ")";
                g_error_free(gerr);
            }
            std::cerr << std::endl;
            return;
        }

        const int smallSize = GetSystemMetrics(SM_CXSMICON);
        const int bigSize   = GetSystemMetrics(SM_CXICON);

        HICON hSmall = renderSvgToHIcon(handle, smallSize);
        HICON hBig   = renderSvgToHIcon(handle, bigSize);
        g_object_unref(handle);

        if (hSmall != nullptr) {
            SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, asLParam(hSmall));
            if (m_hIconSmall != nullptr) {
                DestroyIcon(m_hIconSmall);
            }
            m_hIconSmall = hSmall;
        }
        if (hBig != nullptr) {
            SendMessageW(m_hwnd, WM_SETICON, ICON_BIG, asLParam(hBig));
            if (m_hIconBig != nullptr) {
                DestroyIcon(m_hIconBig);
            }
            m_hIconBig = hBig;
        }

        std::cout << "[Win32Window] Window icon set from " << mainIconPath << std::endl;
    }

    // -------- Win32-specific methods (called by WindowBase via CRTP) --------

    bool createPopup(int screenX, int screenY, fpx_t width, fpx_t height, HWND parentHwnd)
    {
        WNDCLASSW wc     = {};
        wc.lpfnWndProc   = DefWindowProcW;
        wc.hInstance     = GetModuleHandle(nullptr);
        wc.lpszClassName = L"PureGlUiPopupClass";
        RegisterClassW(&wc);

        // WS_POPUP: no decorations. WS_EX_TOPMOST: always on top. WS_EX_TOOLWINDOW: no taskbar entry.
        // Created hidden - caller renders first frame then calls show() to avoid flash.
        m_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                 L"PureGlUiPopupClass",
                                 L"",
                                 WS_POPUP,
                                 screenX,
                                 screenY,
                                 static_cast<int>(width),
                                 static_cast<int>(height),
                                 parentHwnd,
                                 nullptr,
                                 wc.hInstance,
                                 nullptr);

        if (!m_hwnd) {
            return false;
        }

        m_hdc                     = GetDC(m_hwnd);
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize                 = sizeof(pfd);
        pfd.nVersion              = 1;
        pfd.dwFlags               = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType            = PFD_TYPE_RGBA;
        pfd.cColorBits            = 32;
        pfd.cDepthBits            = 24;
        pfd.iLayerType            = PFD_MAIN_PLANE;
        int pf                    = ChoosePixelFormat(m_hdc, &pfd);
        SetPixelFormat(m_hdc, pf, &pfd);

        // Share GL context with parent so textures/shaders are accessible
        m_hglrc             = wglCreateContext(m_hdc);
        HGLRC parentContext = wglGetCurrentContext();
        if (parentContext) {
            wglShareLists(parentContext, m_hglrc);
        }
        wglMakeCurrent(m_hdc, m_hglrc);

        return true;
    }

    void applyRoundedCorners()
    {
        if (!m_hwnd) {
            return;
        }
        if (!m_rounded || m_cornerRadius <= 0) {
            // Remove custom region - restore default window shape
            SetWindowRgn(m_hwnd, NULL, TRUE);
            return;
        }
        // Win32 API requires int
        auto radius = static_cast<int>(m_cornerRadius);
        HRGN rgn    = CreateRoundRectRgn(0,
                                      0,
                                      static_cast<int>(m_bound.w) + 1,
                                      static_cast<int>(m_bound.h) + 1,
                                      radius * 2,
                                      radius * 2);
        SetWindowRgn(m_hwnd, rgn, TRUE);
    }

    // Win32 sends WM_SIZE/WM_CLOSE directly to the WndProc (not posted to the queue).
    // WM_SIZE arrives inside a modal resize loop, so PostMessage cannot relay it to pollEvent().
    // Instead, we update m_bound directly and let pollEvent() detect the change.
    // WM_CLOSE is re-posted as WM_APP_CLOSE so pollEvent() can pick it up.
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        auto * self = fromUserData(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        switch (msg) {
        case WM_SIZE:
            if (self != nullptr) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                self->syncDimensions(static_cast<fpx_t>(rect.right - rect.left),
                                     static_cast<fpx_t>(rect.bottom - rect.top));
            }
            break;
        case WM_MOVE:
            // Track the client origin (like WM_SIZE tracks the size - both are
            // SENT inside modal loops, so pollEvent's change detector picks the
            // update up). A move-only drag then fires an event and the session
            // persists the new position independently of any resize.
            if (self != nullptr) {
                self->m_bound.x = static_cast<fpx_t>(static_cast<short>(LOWORD(lParam)));
                self->m_bound.y = static_cast<fpx_t>(static_cast<short>(HIWORD(lParam)));
            }
            break;
        case WM_CLOSE:
            PostMessageW(hwnd, WM_APP_CLOSE, 0, 0);
            return 0; // Don't let DefWindowProc call DestroyWindow yet
        default: break;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

private:
    // BITMAPV5HEADER is the extended first member of the BITMAPINFO layout;
    // CreateDIBSection reads it through BITMAPINFO* (Win32 idiom). Project rule:
    // no reinterpret_cast - the two-step static_cast through void* is the same
    // defined operation, and this named helper is its single home.
    static BITMAPINFO * asBitmapInfo(BITMAPV5HEADER * header)
    {
        return static_cast<BITMAPINFO *>(static_cast<void *>(header));
    }

    // GWLP_USERDATA stashes `this` for the static windowProc. LONG_PTR is an
    // integer, so static_cast cannot express the conversion - these named
    // helpers are the sanctioned reinterpret_cast exception (int<->ptr, Win32
    // API contract).
    static LONG_PTR asUserData(Win32Window * self)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<LONG_PTR>(self);
    }

    static Win32Window * fromUserData(LONG_PTR userData)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,performance-no-int-to-ptr)
        return reinterpret_cast<Win32Window *>(userData);
    }

    // WM_SETICON passes the HICON through the integer LPARAM - same sanctioned
    // int<->ptr exception as above.
    static LPARAM asLParam(HICON icon)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        return reinterpret_cast<LPARAM>(icon);
    }

    // Render an SVG at a given size into a Windows HICON with alpha.
    // Cairo's CAIRO_FORMAT_ARGB32 on little-endian is B,G,R,A in memory -
    // matches the BGRA layout a 32-bit DIB with BI_BITFIELDS expects, so
    // we can copy rows directly (accounting for Cairo's stride).
    static HICON renderSvgToHIcon(RsvgHandle * handle, int size)
    {
        cairo_surface_t * surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size, size);
        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            return nullptr;
        }
        cairo_t * cr = cairo_create(surface);
#if LIBRSVG_CHECK_VERSION(2, 46, 0)
        const RsvgRectangle viewport = { 0, 0, static_cast<double>(size), static_cast<double>(size) };
        GError *            err      = nullptr;
        if (rsvg_handle_render_document(handle, cr, &viewport, &err) == 0) {
            if (err != nullptr) {
                g_error_free(err);
            }
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            return nullptr;
        }
#else
        RsvgDimensionData dim = {};
        rsvg_handle_get_dimensions(handle, &dim);
        if (dim.width > 0 && dim.height > 0) {
            cairo_scale(cr, static_cast<double>(size) / dim.width, static_cast<double>(size) / dim.height);
        }
        rsvg_handle_render_cairo(handle, cr);
#endif
        cairo_surface_flush(surface);

        const uint8_t * srcData = cairo_image_surface_get_data(surface);
        const int       stride  = cairo_image_surface_get_stride(surface);

        BITMAPV5HEADER bi = {};
        bi.bV5Size        = sizeof(bi);
        bi.bV5Width       = size;
        bi.bV5Height      = -size; // negative = top-down
        bi.bV5Planes      = 1;
        bi.bV5BitCount    = 32;
        bi.bV5Compression = BI_BITFIELDS;
        bi.bV5RedMask     = 0x00FF0000;
        bi.bV5GreenMask   = 0x0000FF00;
        bi.bV5BlueMask    = 0x000000FF;
        bi.bV5AlphaMask   = 0xFF000000;

        HDC     screenDC = GetDC(nullptr);
        void *  bits     = nullptr;
        HBITMAP colorBmp = CreateDIBSection(screenDC, asBitmapInfo(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
        ReleaseDC(nullptr, screenDC);

        HICON hIcon = nullptr;
        if (colorBmp != nullptr && bits != nullptr) {
            auto * dst = static_cast<uint8_t *>(bits);
            for (int row = 0; row < size; ++row) {
                std::memcpy(dst + row * size * 4, srcData + row * stride, size * 4);
            }

            HBITMAP maskBmp = CreateBitmap(size, size, 1, 1, nullptr);

            ICONINFO ii = {};
            ii.fIcon    = TRUE;
            ii.hbmColor = colorBmp;
            ii.hbmMask  = maskBmp;
            hIcon       = CreateIconIndirect(&ii);

            DeleteObject(maskBmp);
        }
        if (colorBmp != nullptr) {
            DeleteObject(colorBmp);
        }

        cairo_destroy(cr);
        cairo_surface_destroy(surface);
        return hIcon;
    }

    HWND  m_hwnd       = nullptr;
    HDC   m_hdc        = nullptr;
    HGLRC m_hglrc      = nullptr;
    HICON m_hIconBig   = nullptr;
    HICON m_hIconSmall = nullptr;
};

} // namespace Ui::Window::Platform
