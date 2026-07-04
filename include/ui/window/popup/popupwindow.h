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

#include "common/bytes.h"
#include "ui/config.h"
#include "ui/interface/ipopuprenderer.h"
#include "ui/render/popup/dialogrenderer.h"
#include "ui/render/popup/popuprenderer.h"
#include "ui/window/eglcontext.h"
#include "ui/window/nativewindow.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <utility>

#if !defined(_WIN32) && !defined(__APPLE__)
#ifdef HAVE_WAYLAND
#include <xdg-shell-client-protocol.h>
#endif
#endif

namespace Ui::Window::Popup {

// Windowing primitives now live in the framework backend.
using Ui::Window::NativeDisplayHandle;
using Ui::Window::NativeWindow;
using Ui::Window::NativeWindowHandle;

/**
 * @brief Popup window for dropdowns and context menus
 *
 * Inherits from NativeWindow (platform alias) and specializes for popup behavior:
 * - No window decorations
 * - Always on top (above parent and all child windows)
 * - Positioned absolutely on screen
 * - Auto-closes on focus loss
 *
 * On Wayland, uses xdg_popup for proper popup semantics.
 * On X11, uses override-redirect or transient windows.
 */
class PopupWindow : public NativeWindow {
public:
    explicit PopupWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID)
        : NativeWindow(subscribe, subscribeId)
    {
    }
    ~PopupWindow() override
    {
        // Destroy the renderer (its dtor makes our context current for its own GL
        // teardown) before the window's GL resources are released.
        m_menuRenderer.reset();
#ifdef HAVE_WAYLAND
        cleanupWayland();
#endif
    }

    // The popup owns its renderer as the concrete type; the base drives it through
    // these overrides, so nothing downcasts. DialogWindow overrides them for its
    // DialogRenderer.
    [[nodiscard]] bool                         hasRenderer() const override { return m_menuRenderer != nullptr; }
    [[nodiscard]] Ui::IRenderer &              activeRenderer() override { return *m_menuRenderer; }
    [[nodiscard]] virtual Ui::IPopupRenderer & popupRenderer() { return *m_menuRenderer; }

    // Concrete menu renderer (menu/submenu popups only).
    [[nodiscard]] Ui::Render::Popup::PopupRenderer & menuRenderer() { return *m_menuRenderer; }

    void move(fpx_t x, fpx_t y) override
    {
        NativeWindow::move(x, y);
        if (hasRenderer()) {
            activeRenderer().move();
        }
    }

    // True when the popup is an X11 child of the main window: the server moves
    // and re-anchors it with the parent, so the shell must NOT manually track
    // window move/resize (doing so double-moves it).
    [[nodiscard]] bool followsParent() const { return m_followsParent; }

    void initRenderer(const Ui::Res::ResManager & resManager, const Ui::Res::Type::menu_t & menu)
    {
        makeCurrent();
        m_menuRenderer = std::make_unique<Ui::Render::Popup::PopupRenderer>([this] { makeCurrent(); },
                                                                            resManager,
                                                                            menu);
        m_menuRenderer->resize(m_bound.w, m_bound.h);
        m_menuRenderer->setAlpha(m_hasAlpha);
    }

#if !defined(_WIN32) && !defined(__APPLE__)

#if defined(HAVE_X11) && !defined(HAVE_WAYLAND)
private:
    /**
     * @brief Create popup window at specified screen coordinates (X11)
     *
     * Only available when building for X11 (not Wayland).
     * Uses inherited members from X11Window base class.
     *
     * For real alpha transparency:
     * 1. We first try to find a 32-bit ARGB visual directly from X11
     * 2. Only if that fails, fall back to EGL-provided visual
     * 3. Alpha only works with depth=32 AND a running compositor
     */
    bool createX11Popup()
    {
        m_display     = m_parent->nativeDisplay();
        m_ownsDisplay = false;

        std::cout << "[PopupWindow] X11 create at (" << m_bound.x << "," << m_bound.y << ") size " << m_bound.w << "x"
                  << m_bound.h << std::endl;

        const int      screen = DefaultScreen(m_display);
        const ::Window root   = RootWindow(m_display, screen);

        auto & cache = eglCache();

        // First popup: full discovery. Subsequent: use cached results.
        XVisualInfo * vi = nullptr;

        if (!cache.searched) {
            // --- First-time discovery (slow path) ---
            cache.searched = true;

            // Detect compositor
            {
                int compMajor {};
                int compMinor {};
                cache.hasCompositor = false;
                if (XCompositeQueryVersion(m_display, &compMajor, &compMinor) != 0) {
                    const Atom     compAtom = XInternAtom(m_display, "_NET_WM_CM_S0", Ui::Window::Platform::X11::False);
                    const ::Window selOwner = XGetSelectionOwner(m_display, compAtom);
                    cache.hasCompositor     = (selOwner != Ui::Window::Platform::X11::None);
                }
                // XWayland: Wayland compositor handles alpha even if X11 atom is absent
                if (!cache.hasCompositor && g_config.isCompositing) {
                    cache.hasCompositor = true;
                }
                std::cout << "[PopupWindow] Compositor present=" << cache.hasCompositor << std::endl;
            }

            // Try to find a 32-bit ARGB visual
            VisualID argbVid = 0;
            if (cache.hasCompositor) {
                argbVid = findArgbVisualId(screen);
            }
            cache.argbVisualId = argbVid;

            if (argbVid != 0) {
                std::cout << "[PopupWindow] Found 32-bit ARGB visual (id=" << argbVid << ")" << std::endl;

                m_context = std::make_unique<Ui::Window::EglContext>();
                if (!m_context->init(m_display)) {
                    return false;
                }

                if (m_context->chooseConfigForVisual(argbVid, true)) {
                    const uint64_t eglVid = m_context->visualId();
                    vi                    = visualInfo(eglVid);

                    if (vi != nullptr && vi->depth >= 32) {
                        m_hasAlpha = true;
                        m_msaa     = m_context->hasMsaa();
                        std::cout << "[PopupWindow] Using EGL 32-bit visual (depth=" << vi->depth << ")" << std::endl;
                    } else {
                        if (vi != nullptr) {
                            XFree(vi);
                            vi = nullptr;
                        }
                        m_context.reset();
                        cache.argbVisualId = 0;
                    }
                } else {
                    std::cout << "[PopupWindow] Falling back to opaque with corner blending" << std::endl;
                    m_context.reset();
                    cache.argbVisualId = 0;
                }
            }

            // If ARGB failed, do opaque discovery and cache results
            if (vi == nullptr) {
                m_context = std::make_unique<Ui::Window::EglContext>();
                if (!m_context->init(m_display)) {
                    return false;
                }

                if (!m_context->chooseConfig(false, true)) {
                    if (!m_context->chooseConfig(false, false)) {
                        return false;
                    }
                }

                m_hasAlpha = false;
                m_msaa     = m_context->hasMsaa();

                // Cache the discovered display+config for reuse by later popups
                m_context->cacheConfig();

                vi = visualInfo(m_context->visualId());
                if (vi == nullptr) {
                    std::cerr << "[PopupWindow] Failed to get X11 visual" << std::endl;
                    return false;
                }
            }
        } else {
            // --- Cached fast path ---
            m_hasCompositor = cache.hasCompositor;

            // Reuse cached display+config (skip init + config search)
            m_context = std::make_unique<Ui::Window::EglContext>();
            if (!m_context->initCachedConfig(m_display)) {
                std::cerr << "[PopupWindow] No cached EGL config available" << std::endl;
                return false;
            }

            m_hasAlpha = false;
            m_msaa     = m_context->hasMsaa();

            vi = visualInfo(m_context->visualId());
            if (vi == nullptr) {
                std::cerr << "[PopupWindow] Failed to get X11 visual from cache" << std::endl;
                return false;
            }
        }

        m_hasCompositor = cache.hasCompositor;
        std::cout << "[PopupWindow] Visual depth=" << vi->depth << " hasAlpha=" << m_hasAlpha
                  << " msaa=" << (m_msaa ? "yes" : "no") << std::endl;

        // Create colormap for the visual
        const Colormap cmap = XCreateColormap(m_display, root, vi->visual, AllocNone);

        // Set window attributes for popup
        XSetWindowAttributes swa;
        swa.colormap   = cmap;
        swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask
                       | StructureNotifyMask | FocusChangeMask;

        swa.override_redirect = Ui::Window::Platform::X11::False;
        swa.save_under        = Ui::Window::Platform::X11::True;
        swa.border_pixel      = 0;

        // Don't set a background - we'll use XSetWindowBackgroundPixmap(Ui::Window::Platform::X11::None) after
        // creation to prevent X11 from painting any background before GL content is shown
        swa.background_pixel = 0;

        // For ARGB visuals, must use root as parent to avoid BadMatch
        const ::Window parentHandle = m_parent->nativeHandle();
        ::Window       winParent    = parentHandle;
        if (vi->depth >= 32 && parentHandle != root) {
            winParent = root;
            std::cout << "[PopupWindow] Using root as parent for ARGB visual" << std::endl;
        }
        // A non-root parent makes this an X11 child window, so the server moves
        // it with the parent automatically - no manual move/resize tracking.
        m_followsParent = (winParent != root);

        m_xWindow = XCreateWindow(m_display,
                                  winParent,
                                  m_bound.x,
                                  m_bound.y,
                                  m_bound.w,
                                  m_bound.h,
                                  0,
                                  vi->depth,
                                  InputOutput,
                                  vi->visual,
                                  CWColormap | CWBackPixel | CWBorderPixel | CWEventMask | CWSaveUnder,
                                  &swa);
        XFree(vi);
        vi = nullptr;

        if (m_xWindow == 0U) {
            std::cerr << "[PopupWindow] Failed to create X11 window" << std::endl;
            return false;
        }

        // Set window type hint for proper stacking
        const Atom wmWindowType     = XInternAtom(m_display, "_NET_WM_WINDOW_TYPE", Ui::Window::Platform::X11::False);
        Atom       wmWindowTypeMenu = XInternAtom(m_display,
                                            "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
                                            Ui::Window::Platform::X11::False);
        XChangeProperty(m_display,
                        m_xWindow,
                        wmWindowType,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        Common::asBytes(&wmWindowTypeMenu),
                        1);

        // Set transient hint for proper window management
        XSetTransientForHint(m_display, m_xWindow, m_parent->nativeHandle());

        // Create EGL surface and context
        if (!m_context->createSurface(m_xWindow)) {
            XDestroyWindow(m_display, m_xWindow);
            m_xWindow = 0;
            return false;
        }

        if (!m_context->createContext()) {
            XDestroyWindow(m_display, m_xWindow);
            m_xWindow = 0;
            return false;
        }

        // Setup GL state
        m_context->makeCurrent();

        if (m_hasAlpha) {
            // Enable premultiplied alpha blending
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

            // Clear to fully transparent
            glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        } else {
            // Opaque background
            auto bg = m_bgColor.toGLRGB();
            glClearColor(bg.at(0), bg.at(1), bg.at(2), 1.0F);
        }

        if (m_msaa) {
            glEnable(GL_MULTISAMPLE);
#ifdef GL_SAMPLE_ALPHA_TO_COVERAGE
            if (m_hasAlpha) {
                glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
            }
#endif
        }

        // Tell X11 not to paint any background - let GL handle everything
        XSetWindowBackgroundPixmap(m_display, m_xWindow, Ui::Window::Platform::X11::None);

        // Do NOT map the window yet. The caller renders the first frame with
        // rounded corners, then calls show(). This avoids a visible flash of
        // the flat/unrounded window before GL content is ready.
        // EGL can render to an unmapped window; swapBuffers buffers the
        // content which becomes visible once the window is mapped.

        std::cout << "[PopupWindow] X11 popup created with alpha=" << m_hasAlpha << " compositor=" << m_hasCompositor
                  << std::endl;
        return true;
    }

    /**
     * @brief Find a 32-bit TrueColor ARGB visual ID
     *
     * For real alpha transparency on X11, we need a visual with:
     * - depth = 32
     * - class = TrueColor
     * - proper RGB masks (indicating alpha channel exists)
     *
     * @param screen X11 screen number
     * @return Visual ID for ARGB visual, or 0 if not found
     */
    VisualID findArgbVisualId(int screen)
    {
        // Get all 32-bit TrueColor visuals for this screen
        XVisualInfo viTemplate;
        viTemplate.screen        = screen;
        viTemplate.depth         = 32;
        viTemplate.c_class       = TrueColor;
        int           numVisuals = 0;
        XVisualInfo * visuals    = XGetVisualInfo(m_display,
                                               VisualScreenMask | VisualDepthMask | VisualClassMask,
                                               &viTemplate,
                                               &numVisuals);

        if (visuals == nullptr || numVisuals == 0) {
            std::cout << "[PopupWindow] No 32-bit TrueColor visuals found" << std::endl;
            return 0;
        }

        std::cout << "[PopupWindow] Found " << numVisuals << " 32-bit TrueColor visuals" << std::endl;

        // Find a visual with ARGB format
        VisualID result = 0;
        for (int i = 0; i < numVisuals; ++i) {
            XVisualInfo * vi = &visuals[i]; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

            // Check for proper RGBA masks (8 bits per component)
            if (vi->bits_per_rgb == 8 && vi->red_mask != 0 && vi->green_mask != 0 && vi->blue_mask != 0) {
                std::cout << "[PopupWindow] Visual " << vi->visualid << ": red=0x" << std::hex << vi->red_mask
                          << " green=0x" << vi->green_mask << " blue=0x" << vi->blue_mask << std::dec
                          << " bits_per_rgb=" << vi->bits_per_rgb << std::endl;

                result = vi->visualid;
                break;
            }
        }

        XFree(visuals);
        return result;
    }

    /**
     * @brief Get XVisualInfo for a visual ID (caller must XFree the result)
     */
    XVisualInfo * visualInfo(VisualID vid)
    {
        XVisualInfo viTemplate;
        viTemplate.visualid = vid;
        int numVisuals      = 0;
        return XGetVisualInfo(m_display, VisualIDMask, &viTemplate, &numVisuals);
    }

public:
#endif // HAVE_X11 && !HAVE_WAYLAND

#ifdef HAVE_WAYLAND
private:
    /**
     * @brief Create popup window at specified screen coordinates (Wayland)
     *
     * Uses inherited members from WaylandWindow base class:
     * - m_display (wl_display*)
     * - m_surface (wl_surface*)
     * - m_context (unique_ptr<Ui::IContext>)
     */
    bool createWaylandPopup()
    {
        m_display     = m_parent->wlDisplay();
        m_ownsDisplay = false;
        m_isWayland   = true;

        std::cout << "[PopupWindow] Wayland create at (" << m_bound.x << "," << m_bound.y << ") size " << m_bound.w
                  << "x" << m_bound.h << std::endl;

        // Create surface for popup (use inherited m_surface)
        m_surface = wl_compositor_create_surface(m_parent->wlCompositor());
        if (!m_surface) {
            std::cerr << "[PopupWindow] Failed to create Wayland surface" << std::endl;
            return false;
        }

        // Create xdg_surface for popup
        m_popupXdgSurface = xdg_wm_base_get_xdg_surface(m_parent->xdgWmBase(), m_surface);
        if (!m_popupXdgSurface) {
            std::cerr << "[PopupWindow] Failed to create xdg_surface" << std::endl;
            return false;
        }
        xdg_surface_add_listener(m_popupXdgSurface, &s_xdgSurfaceListener, this);

        // Create positioner for popup placement
        xdg_positioner * positioner = xdg_wm_base_create_positioner(m_parent->xdgWmBase());
        if (!positioner) {
            std::cerr << "[PopupWindow] Failed to create xdg_positioner" << std::endl;
            return false;
        }

        // Set popup size and anchor in logical (surface) coordinates
        const int logicalW = static_cast<int>(m_bound.w / g_config.scale);
        const int logicalH = static_cast<int>(m_bound.h / g_config.scale);
        xdg_positioner_set_size(positioner, logicalW, logicalH);
        const int anchorX = static_cast<int>(m_bound.x / g_config.scale);
        const int anchorY = static_cast<int>(m_bound.y / g_config.scale);
        xdg_positioner_set_anchor_rect(positioner, anchorX, anchorY, 1, 1);
        xdg_positioner_set_anchor(positioner, XDG_POSITIONER_ANCHOR_TOP_LEFT);
        xdg_positioner_set_gravity(positioner, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT);
        xdg_positioner_set_constraint_adjustment(
        positioner,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y
        | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X | XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);

        // Create popup
        m_xdgPopup = xdg_surface_get_popup(m_popupXdgSurface, m_parent->xdgSurface(), positioner);
        xdg_positioner_destroy(positioner);

        if (!m_xdgPopup) {
            std::cerr << "[PopupWindow] Failed to create xdg_popup" << std::endl;
            return false;
        }
        xdg_popup_add_listener(m_xdgPopup, &s_xdgPopupListener, this);

        // Tell compositor our buffer is pre-scaled
        wl_surface_set_buffer_scale(m_surface, static_cast<int>(g_config.scale));

        // Commit to trigger configure
        wl_surface_commit(m_surface);

        // Wait for configure
        while (!m_popupConfigured) {
            wl_display_roundtrip(m_display);
        }

        // Initialize EGL context (use inherited m_context)
        m_context = Ui::Window::EglContext::create(m_display, Ui::Window::EglPlatform::Wayland);
        if (!m_context) {
            std::cerr << "[PopupWindow] Failed to init EGL for Wayland" << std::endl;
            return false;
        }

        // Request alpha + MSAA
        if (!m_context->chooseConfig(true, true)) {
            std::cerr << "[PopupWindow] Failed to choose EGL config" << std::endl;
            return false;
        }

        m_hasAlpha = m_context->hasAlpha();
        m_msaa     = m_context->hasMsaa();

        // Create EGL surface from wl_surface
        if (!m_context->createSurface(m_surface, m_bound.w, m_bound.h)) {
            std::cerr << "[PopupWindow] Failed to create EGL surface" << std::endl;
            return false;
        }

        if (!m_context->createContext()) {
            std::cerr << "[PopupWindow] Failed to create EGL context" << std::endl;
            return false;
        }

        // Wayland always has compositor, alpha works natively
        m_hasCompositor = true;

        // Setup GL state
        m_context->makeCurrent();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        if (m_msaa) {
            glEnable(GL_MULTISAMPLE);
#ifdef GL_SAMPLE_ALPHA_TO_COVERAGE
            glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
#endif
        }

        glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFinish();
        m_context->swapBuffers();

        wl_surface_commit(m_surface);
        wl_display_flush(m_display);

        std::cout << "[PopupWindow] Wayland popup created with alpha=" << m_hasAlpha
                  << " msaa=" << (m_msaa ? "yes" : "no") << std::endl;
        return true;
    }

public:
    /**
     * @brief Check if popup was dismissed by compositor
     */
    [[nodiscard]] bool wasDismissed() const { return m_popupDismissed; }

    /**
     * @brief Get wl_surface for event registration
     */
    [[nodiscard]] wl_surface * popupSurface() const { return m_surface; }
#endif // HAVE_WAYLAND

#endif // !defined(_WIN32) && !defined(__APPLE__)

    /**
     * @brief Create popup window with parent reference
     *
     * Platform-agnostic entry point. Extracts native handles from parent.
     * Position and background must be set before calling (setPosition, setBackground).
     *
     * @param parent Parent window (must outlive this popup)
     * @param width Popup width in physical pixels
     * @param height Popup height in physical pixels
     * @return true on success
     */
    bool create(NativeWindow & parent, fpx_t width, fpx_t height)
    {
        m_parent  = &parent;
        m_bound.w = width;
        m_bound.h = height;

#if defined(_WIN32)
        return createWin32Popup();
#elif defined(__APPLE__)
        return createMacOsPopup();
#elif defined(HAVE_WAYLAND)
        return createWaylandPopup();
#elif defined(HAVE_X11)
        return createX11Popup();
#else
        std::cerr << "[PopupWindow] No supported window system" << std::endl;
        return false;
#endif
    }

    void setBackground(const Ui::Color & color) override { m_bgColor = color; }

    [[nodiscard]] Ui::Color background() const { return m_bgColor; }

    void clear() override
    {
        if (m_hasAlpha) {
            glClearColor(0.0F, 0.0F, 0.0F, 0.0F);
        } else {
            auto bg = m_bgColor.toGLRGB();
            glClearColor(bg.at(0), bg.at(1), bg.at(2), 1.0F);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // NOLINT(hicpp-signed-bitwise)
    }

    [[nodiscard]] bool hasAlpha() const { return m_hasAlpha; }
    [[nodiscard]] bool hasCompositor() const { return m_hasCompositor; }
    [[nodiscard]] bool msaaVisual() const { return m_msaa; }
    [[nodiscard]] bool isWayland() const { return m_isWayland; }

    void setRoundedCorners(bool enabled, fpx_t radius) override
    {
        m_rounded     = enabled;
        const fpx_t r = std::max(0.0F, radius);
        m_cornerRadii = { r, r, r, r };
        // On Wayland, no shape mask needed - alpha compositing handles rounded corners natively
    }

    /**
     * @brief Set individual corner radii (TL, TR, BR, BL)
     * Values < 0.5 are treated as zero (not visually significant)
     */
    void setCornerRadii(const Ui::Res::Type::border_t & radii)
    {
        m_cornerRadii = radii;
        m_rounded     = (radii.topLeft > 0 || radii.topRight > 0 || radii.bottomRight > 0 || radii.bottomLeft > 0);
#if defined(__APPLE__)
        applyCornerMask(radii);
#endif
    }

    [[nodiscard]] const Ui::Res::Type::border_t & cornerRadii() const { return m_cornerRadii; }

    [[nodiscard]] bool containsPoint(fpx_t px, fpx_t py) const
    {
        if (px < 0 || px >= m_bound.w || py < 0 || py >= m_bound.h) {
            return false;
        }
        if (!m_rounded) {
            return true;
        }

        auto smoothStep = [](fpx_t edge0, fpx_t edge1, fpx_t x) {
            if (edge1 <= edge0) {
                return (x >= edge1) ? 1.0F : 0.0F;
            }
            const fpx_t t = (x - edge0) / (edge1 - edge0);
            if (t <= 0.0F) {
                return 0.0F;
            }
            if (t >= 1.0F) {
                return 1.0F;
            }
            return t * t * (3.0F - 2.0F * t);
        };

        if (px >= m_cornerRadii.topLeft && px <= m_bound.w - m_cornerRadii.topRight && py >= m_cornerRadii.topLeft
            && py <= m_bound.h - m_cornerRadii.bottomRight) {
            return true;
        }

        fpx_t alpha = 0.0F;

        if (m_borderAA <= 0.0F) {
            if (px >= m_cornerRadii.topLeft && px <= m_bound.w - m_cornerRadii.topRight) {
                if (py < m_cornerRadii.topLeft) {
                    return false;
                }
                if (py > m_bound.h - m_cornerRadii.bottomRight) {
                    return false;
                }
            }
            if (py >= m_cornerRadii.topLeft && py <= m_bound.h - m_cornerRadii.bottomRight) {
                if (px < m_cornerRadii.topLeft) {
                    return false;
                }
                if (px > m_bound.w - m_cornerRadii.topRight) {
                    return false;
                }
            }
            auto cornerHard = [&](fpx_t cx, fpx_t cy, fpx_t r) {
                const fpx_t dx = px - cx;
                const fpx_t dy = py - cy;
                return (dx * dx + dy * dy) <= (r * r);
            };
            if (px < m_cornerRadii.topLeft && py < m_cornerRadii.topLeft && m_cornerRadii.topLeft > 0.0F) {
                return cornerHard(m_cornerRadii.topLeft, m_cornerRadii.topLeft, m_cornerRadii.topLeft);
            }
            if (px > m_bound.w - m_cornerRadii.topRight && py < m_cornerRadii.topLeft
                && m_cornerRadii.topRight > 0.0F) {
                return cornerHard(m_bound.w - m_cornerRadii.topRight, m_cornerRadii.topLeft, m_cornerRadii.topRight);
            }
            if (px > m_bound.w - m_cornerRadii.bottomRight && py > m_bound.h - m_cornerRadii.bottomRight
                && m_cornerRadii.bottomRight > 0.0F) {
                return cornerHard(m_bound.w - m_cornerRadii.bottomRight,
                                  m_bound.h - m_cornerRadii.bottomRight,
                                  m_cornerRadii.bottomRight);
            }
            if (px < m_cornerRadii.bottomLeft && py > m_bound.h - m_cornerRadii.bottomLeft
                && m_cornerRadii.bottomLeft > 0.0F) {
                return cornerHard(m_cornerRadii.bottomLeft,
                                  m_bound.h - m_cornerRadii.bottomLeft,
                                  m_cornerRadii.bottomLeft);
            }
            return true;
        }

        if (px >= m_cornerRadii.topLeft && px <= m_bound.w - m_cornerRadii.topRight) {
            if (py < m_cornerRadii.topLeft) {
                alpha = std::max(alpha,
                                 1.0F
                                 - smoothStep(m_cornerRadii.topLeft - m_borderAA,
                                              m_cornerRadii.topLeft + m_borderAA,
                                              m_cornerRadii.topLeft - py));
            }
            if (py > m_bound.h - m_cornerRadii.bottomRight) {
                alpha = std::max(alpha,
                                 1.0F
                                 - smoothStep(m_cornerRadii.bottomRight - m_borderAA,
                                              m_cornerRadii.bottomRight + m_borderAA,
                                              py - (m_bound.h - m_cornerRadii.bottomRight)));
            }
        }
        if (py >= m_cornerRadii.topLeft && py <= m_bound.h - m_cornerRadii.bottomRight) {
            if (px < m_cornerRadii.topLeft) {
                alpha = std::max(alpha,
                                 1.0F
                                 - smoothStep(m_cornerRadii.topLeft - m_borderAA,
                                              m_cornerRadii.topLeft + m_borderAA,
                                              m_cornerRadii.topLeft - px));
            }
            if (px > m_bound.w - m_cornerRadii.topRight) {
                alpha = std::max(alpha,
                                 1.0F
                                 - smoothStep(m_cornerRadii.topRight - m_borderAA,
                                              m_cornerRadii.topRight + m_borderAA,
                                              px - (m_bound.w - m_cornerRadii.topRight)));
            }
        }

        auto cornerMask = [&](fpx_t cx, fpx_t cy, fpx_t r) {
            const fpx_t dx   = px - cx;
            const fpx_t dy   = py - cy;
            const fpx_t dist = std::sqrt(dx * dx + dy * dy);
            return 1.0F - smoothStep(r - m_borderAA, r + m_borderAA, dist);
        };

        if (px < m_cornerRadii.topLeft && py < m_cornerRadii.topLeft && m_cornerRadii.topLeft > 0.0F) {
            alpha = std::max(alpha, cornerMask(m_cornerRadii.topLeft, m_cornerRadii.topLeft, m_cornerRadii.topLeft));
        }
        if (px > m_bound.w - m_cornerRadii.topRight && py < m_cornerRadii.topLeft && m_cornerRadii.topRight > 0.0F) {
            alpha = std::max(
            alpha,
            cornerMask(m_bound.w - m_cornerRadii.topRight, m_cornerRadii.topLeft, m_cornerRadii.topRight));
        }
        if (px > m_bound.w - m_cornerRadii.bottomRight && py > m_bound.h - m_cornerRadii.bottomRight
            && m_cornerRadii.bottomRight > 0.0F) {
            alpha = std::max(alpha,
                             cornerMask(m_bound.w - m_cornerRadii.bottomRight,
                                        m_bound.h - m_cornerRadii.bottomRight,
                                        m_cornerRadii.bottomRight));
        }
        if (px < m_cornerRadii.bottomLeft && py > m_bound.h - m_cornerRadii.bottomLeft
            && m_cornerRadii.bottomLeft > 0.0F) {
            alpha = std::max(
            alpha,
            cornerMask(m_cornerRadii.bottomLeft, m_bound.h - m_cornerRadii.bottomLeft, m_cornerRadii.bottomLeft));
        }

        return alpha > 0.5F;
    }

    [[nodiscard]] bool containsPoint(int px, int py) const
    {
        return containsPoint(static_cast<fpx_t>(px), static_cast<fpx_t>(py));
    }

#if defined(_WIN32)
    bool createWin32Popup()
    {
        // Convert relative-to-parent position to screen coordinates
        int parentScreenX {};
        int parentScreenY {};
        m_parent->screenPosition(parentScreenX, parentScreenY);
        const int screenX = parentScreenX + static_cast<int>(m_bound.x);
        const int screenY = parentScreenY + static_cast<int>(m_bound.y);

        if (!createPopup(screenX, screenY, m_bound.w, m_bound.h, m_parent->nativeHandle())) {
            std::cerr << "[PopupWindow] Failed to create Win32 popup" << std::endl;
            return false;
        }

        // Opaque popup with corner blending
        m_hasAlpha = false;
        auto bg    = m_bgColor.toGLRGB();
        glClearColor(bg.at(0), bg.at(1), bg.at(2), 1.0F);

        std::cout << "[PopupWindow] Win32 popup created at (" << screenX << "," << screenY << ") size " << m_bound.w
                  << "x" << m_bound.h << std::endl;
        return true;
    }
#endif // _WIN32

#if defined(__APPLE__)
    bool createMacOsPopup()
    {
        int parentScreenX {};
        int parentScreenY {};
        m_parent->screenPosition(parentScreenX, parentScreenY);
        const int screenX = parentScreenX + static_cast<int>(m_bound.x);
        const int screenY = parentScreenY + static_cast<int>(m_bound.y);

        // NativeWindowHandle on macOS is NSView*; resolve its NSWindow for addChildWindow:.
        NSWindow * parentNs = [m_parent->nativeHandle() window];
        if (!createAsPopup(screenX, screenY, m_bound.w, m_bound.h, parentNs)) {
            std::cerr << "[PopupWindow] Failed to create macOS popup" << std::endl;
            return false;
        }

        // Apply per-corner shape via CAShapeLayer mask on the contentView.
        // The shader renders the popup opaquely and the mask defines the visible shape
        // (including flat corners), so we don't rely on shader alpha cutouts that
        // NSOpenGLView would otherwise render as black.
        applyCornerMask(m_cornerRadii);

        m_hasAlpha = false;
        auto bg    = m_bgColor.toGLRGB();
        glClearColor(bg.at(0), bg.at(1), bg.at(2), 1.0F);

        std::cout << "[PopupWindow] macOS popup created at (" << screenX << "," << screenY << ") size " << m_bound.w
                  << "x" << m_bound.h << std::endl;
        return true;
    }
#endif // __APPLE__

private:
    // Prevent direct use of base Ui::IWindow::create() - use create(NativeWindow &, fpx_t, fpx_t)
    bool create(fpx_t /*width*/,
                fpx_t /*height*/,
                NativeDisplayHandle /*display*/,
                NativeWindowHandle /*parentWindow*/,
                const std::string & /*title*/) override
    {
        return false;
    }

    NativeWindow * m_parent = nullptr;

#if !defined(_WIN32) && !defined(__APPLE__)

#ifdef HAVE_WAYLAND
    // Wayland popup-specific members (base class provides m_display, m_surface, m_context)
    xdg_surface * m_popupXdgSurface = nullptr;
    xdg_popup *   m_xdgPopup        = nullptr;
    bool          m_popupConfigured = false;
    bool          m_popupDismissed  = false;

    void cleanupWayland()
    {
        if (m_xdgPopup) {
            xdg_popup_destroy(m_xdgPopup);
            m_xdgPopup = nullptr;
        }
        if (m_popupXdgSurface) {
            xdg_surface_destroy(m_popupXdgSurface);
            m_popupXdgSurface = nullptr;
        }
        // m_surface cleanup is handled by WaylandWindow base class destructor
    }

    // Wayland xdg_surface listener for popup
    static void xdgSurfaceConfigure(void * data, xdg_surface * surface, uint32_t serial)
    {
        auto * self = static_cast<PopupWindow *>(data);
        xdg_surface_ack_configure(surface, serial);
        self->m_popupConfigured = true;
    }

    static constexpr xdg_surface_listener s_xdgSurfaceListener = { xdgSurfaceConfigure };

    // Wayland xdg_popup listener
    static void
    xdgPopupConfigure(void * data, xdg_popup * /*popup*/, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        auto * self = static_cast<PopupWindow *>(data);
        // Configure sends logical coords; convert to physical (app works in physical pixels)
        if (width > 0 && height > 0) {
            self->m_bound.w = width * g_config.scale;
            self->m_bound.h = height * g_config.scale;
        }
        self->m_bound.x = x * g_config.scale;
        self->m_bound.y = y * g_config.scale;
    }

    static void xdgPopupDone(void * data, xdg_popup * /*popup*/)
    {
        auto * self            = static_cast<PopupWindow *>(data);
        self->m_popupDismissed = true;
    }

    static void xdgPopupRepositioned(void * /*data*/, xdg_popup * /*popup*/, uint32_t /*token*/)
    {
        // Handle repositioning if needed
    }

    static constexpr xdg_popup_listener s_xdgPopupListener = { xdgPopupConfigure, xdgPopupDone, xdgPopupRepositioned };
#endif // HAVE_WAYLAND
#endif // !defined(_WIN32) && !defined(__APPLE__)

    bool m_hasAlpha      = false;
    bool m_hasCompositor = false;
    bool m_msaa          = false;
    bool m_isWayland     = false;
    bool m_followsParent = false; // parented to the main window (X11 child) -> auto-tracks parent move/resize

    // The popup owns its renderer as the concrete type (null for a DialogWindow,
    // which owns a DialogRenderer instead).
    std::unique_ptr<Ui::Render::Popup::PopupRenderer> m_menuRenderer;

    // Static cache for EGL config discovery results (survives popup destroy/recreate)
#if defined(HAVE_X11) && !defined(HAVE_WAYLAND)
    struct alignas(16) EglCache final {
        VisualID argbVisualId  = 0;     // Cached ARGB visual (0 = not found)
        bool     searched      = false; // Has the ARGB visual search been done?
        bool     hasCompositor = false; // Cached compositor check result
    };
    static EglCache & eglCache()
    {
        static EglCache cache;
        return cache;
    }
#endif // HAVE_X11 && !HAVE_WAYLAND

    Ui::Res::Type::border_t m_cornerRadii {};
    fpx_t                   m_borderAA = 1.0F;

    Ui::Color m_bgColor;
};

} // namespace Ui::Window::Popup
