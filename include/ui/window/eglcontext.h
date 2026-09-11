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
#include "common/noncopyable.h"
#include "ui/interface/icontext.h"
#include "ui/window/nativedisplayhandle.h"

#include <array>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#ifdef __linux__
#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifdef HAVE_WAYLAND
#include <wayland-client.h>
#include <wayland-egl.h>
#endif

// EGL platform extension constants (may not be in older headers)
#ifndef EGL_PLATFORM_X11_EXT
#define EGL_PLATFORM_X11_EXT 0x31D5
#endif
#ifndef EGL_PLATFORM_WAYLAND_EXT
#define EGL_PLATFORM_WAYLAND_EXT 0x31D8
#endif

#endif // __linux__

namespace Ui::Window {

// Flip to true to trace EGL setup: platform, version, which config each surface
// settled on and why. Every popup builds its own context, so this repeats per
// menu open rather than once at startup.
constexpr bool EGL_DEBUG = false;

/**
 * @brief Platform type for EGL context
 */
enum class EglPlatform : unsigned char { X11, Wayland };

/**
 * @brief EGL-based OpenGL context for Linux (X11 and Wayland)
 *
 * Provides OpenGL context via EGL, supporting:
 * - Alpha channel for window transparency
 * - MSAA for anti-aliasing
 * - Compatibility profile for legacy GL support
 * - Both X11 and Wayland platforms
 */
class EglContext final : public Ui::IContext, private Common::NonCopyable {
public:
    EglContext() = default;
    ~EglContext() override { cleanup(); }

    /**
     * @brief Factory method to create and initialize EGL context for a platform
     * @param display Native display (Display* for X11, wl_display* for Wayland)
     * @param platform Platform type
     * @return Initialized EglContext or nullptr on failure
     */
    static std::unique_ptr<EglContext> create(Ui::Window::NativeDisplayHandle display, EglPlatform platform)
    {
        auto ctx = std::make_unique<EglContext>();
        if (!ctx->init(display, platform)) {
            return nullptr;
        }
        return ctx;
    }

    /**
     * @brief Initialize EGL with X11 display (backward compatible)
     */
    bool init(Ui::Window::NativeDisplayHandle display) override { return init(display, EglPlatform::X11); }

    /**
     * @brief Mark this context as the owner of the display connection. Only the
     * owner terminates the shared EGLDisplay on teardown (see cleanup()).
     */
    void setOwnsDisplay(bool ownsDisplay) override
    {
#ifdef __linux__
        m_ownsDisplay = ownsDisplay;
#else
        (void)ownsDisplay;
#endif
    }

    /**
     * @brief Cache the current display+config so a sibling context (e.g. the next
     * popup) can skip the slow eglInitialize + config search.
     */
    void cacheConfig() override
    {
#ifdef __linux__
        s_cachedDisplay = m_eglDisplay;
        s_cachedConfig  = m_eglConfig;
        s_cachedMsaa    = m_hasMsaa;
        s_cachedValid   = true;
#endif
    }

    /**
     * @brief Initialize from the cached display+config. Returns false if nothing
     * has been cached yet.
     */
    bool initCachedConfig(Ui::Window::NativeDisplayHandle display) override
    {
#ifdef __linux__
        if (!s_cachedValid) {
            return false;
        }
        return initWithCachedConfig(s_cachedDisplay, s_cachedConfig, s_cachedMsaa, false, display);
#else
        (void)display;
        return false;
#endif
    }

#ifdef __linux__
    /**
     * @brief Initialize EGL with a pre-existing display and config
     *
     * Skips eglInitialize() and config search entirely. Used when
     * the display and config have already been discovered and cached.
     */
    bool initWithCachedConfig(EGLDisplay                      eglDisplay,
                              EGLConfig                       eglConfig,
                              bool                            hasMsaa,
                              bool                            hasAlpha,
                              Ui::Window::NativeDisplayHandle display)
    {
        m_eglDisplay    = eglDisplay;
        m_eglConfig     = eglConfig;
        m_hasMsaa       = hasMsaa;
        m_hasAlpha      = hasAlpha;
        m_platform      = EglPlatform::X11;
        m_nativeDisplay = display;
        m_initialized   = true;
        return true;
    }
#endif

    /**
     * @brief Initialize EGL with specified platform
     * @param display Native display (Display* for X11, wl_display* for Wayland)
     * @param platform Platform type
     */
    bool init(Ui::Window::NativeDisplayHandle display, EglPlatform platform)
    {
#ifdef __linux__
        if (display == nullptr) {
            std::cerr << "[EglContext] Null display" << std::endl;
            return false;
        }

        m_platform      = platform;
        m_nativeDisplay = display;

        // Get EGL display based on platform. eglGetProcAddress returns a generic
        // function pointer; converting to the typed PFN is a function-pointer cast,
        // which static_cast cannot express - sanctioned reinterpret_cast exception.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        auto eglGetPlatformDisplayEXT = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
        eglGetProcAddress("eglGetPlatformDisplayEXT"));

        if (platform == EglPlatform::Wayland) {
#ifdef HAVE_WAYLAND
            if (eglGetPlatformDisplayEXT != nullptr) {
                m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_WAYLAND_EXT, m_nativeDisplay, nullptr);
            } else {
                std::cerr << "[EglContext] eglGetPlatformDisplayEXT not available for Wayland" << std::endl;
                return false;
            }
            if constexpr (EGL_DEBUG) {
                std::cout << "[EglContext] Using Wayland platform" << std::endl;
            }
#else
            std::cerr << "[EglContext] Wayland support not compiled in" << std::endl;
            return false;
#endif
        } else {
#ifdef HAVE_X11
            if (eglGetPlatformDisplayEXT != nullptr) {
                m_eglDisplay = eglGetPlatformDisplayEXT(EGL_PLATFORM_X11_EXT, m_nativeDisplay, nullptr);
            } else {
                m_eglDisplay = eglGetDisplay(static_cast<EGLNativeDisplayType>(m_nativeDisplay));
            }
            if constexpr (EGL_DEBUG) {
                std::cout << "[EglContext] Using X11 platform" << std::endl;
            }
#else
            std::cerr << "[EglContext] X11 support not compiled in" << std::endl;
            return false;
#endif
        }

        if (m_eglDisplay == EGL_NO_DISPLAY) {
            std::cerr << "[EglContext] eglGetDisplay failed" << std::endl;
            return false;
        }

        EGLint major = 0;
        EGLint minor = 0;
        if (eglInitialize(m_eglDisplay, &major, &minor) == 0U) {
            std::cerr << "[EglContext] eglInitialize failed" << std::endl;
            return false;
        }
        if constexpr (EGL_DEBUG) {
            std::cout << "[EglContext] EGL version: " << major << "." << minor << std::endl;
        }

        if (eglBindAPI(EGL_OPENGL_API) == 0U) {
            std::cerr << "[EglContext] eglBindAPI(EGL_OPENGL_API) failed" << std::endl;
            return false;
        }

        m_initialized = true;
        return true;
#else
        (void)display;
        (void)platform;
        return false;
#endif
    }

    bool chooseConfig(bool wantAlpha, bool wantMsaa) override
    {
#ifdef __linux__
        if (!m_initialized) {
            return false;
        }

        m_hasAlpha = false;
        m_hasMsaa  = false;

        // Try configs in priority order
        struct alignas(32) ConfigAttempt {
            bool             alpha;
            bool             msaa;
            std::string_view desc;
            bool             stencil = true;
        };

        std::vector<ConfigAttempt> attempts;
        if (wantAlpha && wantMsaa) {
            attempts = { { true, true, "MSAA+alpha" },
                         { true, false, "alpha" },
                         { false, true, "MSAA" },
                         { false,
                           false,
                           "basic" } };
        } else if (wantAlpha) {
            attempts = { { true, false, "alpha" }, { false, false, "basic" } };
        } else if (wantMsaa) {
            attempts = { { false, true, "MSAA" }, { false, false, "basic" } };
        } else {
            attempts = { { false, false, "basic" } };
        }
        // A driver with no stencil config must still open a window
        attempts.emplace_back(ConfigAttempt { false, false, "basic without stencil", false });

        for (const auto & attempt : attempts) {
            std::vector<EGLint> attribs = { EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                                            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                            EGL_RED_SIZE,        8,
                                            EGL_GREEN_SIZE,      8,
                                            EGL_BLUE_SIZE,       8,
                                            EGL_DEPTH_SIZE,      24 };
            // Stencil is asked for here because every child content surface
            // inherits this visual, and a surface cannot win one back later
            if (attempt.stencil) {
                attribs.emplace_back(EGL_STENCIL_SIZE);
                attribs.emplace_back(8);
            }
            if (attempt.alpha) {
                attribs.emplace_back(EGL_ALPHA_SIZE);
                attribs.emplace_back(8);
            }
            if (attempt.msaa) {
                attribs.emplace_back(EGL_SAMPLE_BUFFERS);
                attribs.emplace_back(1);
                attribs.emplace_back(EGL_SAMPLES);
                attribs.emplace_back(4);
            }
            attribs.emplace_back(EGL_NONE);

            EGLint numConfigs = 0;
            if (eglChooseConfig(m_eglDisplay, attribs.data(), &m_eglConfig, 1, &numConfigs) != 0U && numConfigs > 0) {
                m_hasAlpha = attempt.alpha;
                m_hasMsaa  = attempt.msaa;
                if constexpr (EGL_DEBUG) {
                    std::cout << "[EglContext] Config: " << attempt.desc << std::endl;
                }
                return true;
            }
        }

        std::cerr << "[EglContext] No suitable config found" << std::endl;
        return false;
#else
        (void)wantAlpha;
        (void)wantMsaa;
        return false;
#endif
    }

    /**
     * @brief Find an EGL config that matches a specific X11 visual ID
     *
     * This is needed for 32-bit ARGB transparency on X11 because EGL's
     * default config selection often returns 24-bit visuals even when
     * requesting EGL_ALPHA_SIZE=8.
     *
     * @param targetVisualId The X11 visual ID to match
     * @param wantMsaa If true, prefer configs with MSAA
     * @return true if a matching config was found
     */
    bool chooseConfigForVisual(uint64_t targetVisualId, bool wantMsaa) override
    {
#ifdef __linux__
        if (!m_initialized || m_platform == EglPlatform::Wayland) {
            return false;
        }

        // Get the total number of configs
        EGLint totalConfigs = 0;
        if (eglGetConfigs(m_eglDisplay, nullptr, 0, &totalConfigs) == 0U || totalConfigs == 0) {
            std::cerr << "[EglContext] Failed to get config count" << std::endl;
            return false;
        }

        if constexpr (EGL_DEBUG) {
            std::cout << "[EglContext] Searching " << totalConfigs << " configs for visual " << targetVisualId
                      << std::endl;
        }

        // Get all configs
        std::vector<EGLConfig> configs(totalConfigs);
        EGLint                 numConfigs = 0;
        if (eglGetConfigs(m_eglDisplay, configs.data(), totalConfigs, &numConfigs) == 0U) {
            std::cerr << "[EglContext] Failed to get configs" << std::endl;
            return false;
        }

        // Find configs matching the target visual ID
        EGLConfig matchedConfig     = nullptr;
        EGLConfig matchedMsaaConfig = nullptr;

        for (EGLint i = 0; i < numConfigs; ++i) {
            EGLint visualId = 0;
            eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_NATIVE_VISUAL_ID, &visualId);

            if (static_cast<uint64_t>(visualId) == targetVisualId) {
                // Check if this config supports window surfaces and OpenGL
                EGLint surfaceType = 0;
                EGLint renderType  = 0;
                eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_SURFACE_TYPE, &surfaceType);
                eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_RENDERABLE_TYPE, &renderType);

                if (Common::Bit::And(surfaceType, EGL_WINDOW_BIT) == 0
                    || Common::Bit::And(renderType, EGL_OPENGL_BIT) == 0) {
                    continue;
                }

                // Check for MSAA
                EGLint samples = 0;
                eglGetConfigAttrib(m_eglDisplay, configs[i], EGL_SAMPLES, &samples);

                if (samples >= 4 && matchedMsaaConfig == nullptr) {
                    matchedMsaaConfig = configs[i];
                    if constexpr (EGL_DEBUG) {
                        std::cout << "[EglContext] Found MSAA config for visual " << targetVisualId
                                  << " (samples=" << samples << ")" << std::endl;
                    }
                }

                if (matchedConfig == nullptr) {
                    matchedConfig = configs[i];
                }

                // If we found both, we can stop
                if (matchedConfig != nullptr && matchedMsaaConfig != nullptr) {
                    break;
                }
            }
        }

        // Prefer MSAA config if requested and available
        if (wantMsaa && matchedMsaaConfig != nullptr) {
            m_eglConfig = matchedMsaaConfig;
            m_hasMsaa   = true;
        } else if (matchedConfig != nullptr) {
            m_eglConfig = matchedConfig;
            m_hasMsaa   = false;
        } else {
            if constexpr (EGL_DEBUG) {
                std::cout << "[EglContext] No config found for visual " << targetVisualId
                          << " (will use software corner blending)" << std::endl;
            }
            return false;
        }

        m_hasAlpha = true; // We're specifically looking for 32-bit ARGB visuals

        if constexpr (EGL_DEBUG) {
            EGLint depth   = 0;
            EGLint alpha   = 0;
            EGLint samples = 0;
            eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_BUFFER_SIZE, &depth);
            eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_ALPHA_SIZE, &alpha);
            eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_SAMPLES, &samples);
            std::cout << "[EglContext] Selected config: buffer=" << depth << " alpha=" << alpha
                      << " samples=" << samples << std::endl;
        }

        return true;
#else
        (void)targetVisualId;
        (void)wantMsaa;
        return false;
#endif
    }

    [[nodiscard]] uint64_t visualId() const override
    {
#ifdef __linux__
        // Visual ID is only meaningful for X11
        if (m_eglConfig == nullptr || m_platform == EglPlatform::Wayland) {
            return 0;
        }
        EGLint vid = 0;
        eglGetConfigAttrib(m_eglDisplay, m_eglConfig, EGL_NATIVE_VISUAL_ID, &vid);
        return static_cast<uint64_t>(vid);
#else
        return 0;
#endif
    }

    /**
     * @brief Create EGL surface from native window
     * @param window Platform-specific native window handle
     * @param width Surface width (required for Wayland)
     * @param height Surface height (required for Wayland)
     */
    bool createSurface(Ui::Window::NativeWindowHandle window, fpx_t width, fpx_t height) override
    {
#ifdef __linux__
        if (!m_initialized || m_eglConfig == nullptr) {
            return false;
        }

        EGLNativeWindowType nativeWindow = 0;

        if (m_platform == EglPlatform::Wayland) {
#ifdef HAVE_WAYLAND
            if (!window) {
                std::cerr << "[EglContext] Null Wayland surface" << std::endl;
                return false;
            }
            if (width <= 0 || height <= 0) {
                std::cerr << "[EglContext] Invalid dimensions for Wayland surface" << std::endl;
                return false;
            }
            m_wlEglWindow = wl_egl_window_create(window, static_cast<int>(width), static_cast<int>(height));
            if (!m_wlEglWindow) {
                std::cerr << "[EglContext] wl_egl_window_create failed" << std::endl;
                return false;
            }
            // EGLNativeWindowType is platform-defined: with X11 headers in the TU it
            // is Window (an integer), so this is int<->ptr - static_cast cannot
            // express it. Sanctioned reinterpret_cast exception.
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            nativeWindow = reinterpret_cast<EGLNativeWindowType>(m_wlEglWindow);
            if constexpr (EGL_DEBUG) {
                std::cout << "[EglContext] Created wl_egl_window " << width << "x" << height << std::endl;
            }
#else
            std::cerr << "[EglContext] Wayland support not compiled in" << std::endl;
            return false;
#endif
        } else {
#if defined(HAVE_X11) && !defined(HAVE_WAYLAND)
            // For X11, window is ::Window (both ::Window and EGLNativeWindowType are unsigned long)
            (void)width;  // Not used for X11, only Wayland
            (void)height; // Not used for X11, only Wayland
            nativeWindow = static_cast<EGLNativeWindowType>(window);
#else
            std::cerr << "[EglContext] X11 surface requested but not available" << std::endl;
            return false;
#endif
        }

        m_eglSurface = eglCreateWindowSurface(m_eglDisplay, m_eglConfig, nativeWindow, nullptr);
        if (m_eglSurface == EGL_NO_SURFACE) {
            std::cerr << "[EglContext] eglCreateWindowSurface failed: 0x" << std::hex << eglGetError() << std::dec
                      << std::endl;
#ifdef HAVE_WAYLAND
            if (m_wlEglWindow) {
                wl_egl_window_destroy(m_wlEglWindow);
                m_wlEglWindow = nullptr;
            }
#endif
            return false;
        }
        return true;
#else
        (void)window;
        (void)width;
        (void)height;
        return false;
#endif
    }

    bool createSurface(Ui::Window::NativeWindowHandle window) override { return createSurface(window, 0, 0); }

    /**
     * @brief Resize the drawable. Only Wayland's wl_egl_window needs this; the
     * X11/EGL surface tracks the window, so it is a no-op there.
     */
    void resize(fpx_t width, fpx_t height) override
    {
#ifdef HAVE_WAYLAND
        if (m_wlEglWindow && m_platform == EglPlatform::Wayland) {
            wl_egl_window_resize(m_wlEglWindow, static_cast<int>(width), static_cast<int>(height), 0, 0);
        }
#else
        (void)width;
        (void)height;
#endif
    }

    bool createContext() override
    {
#ifdef __linux__
        if (!m_initialized || m_eglConfig == nullptr) {
            return false;
        }

        // Request OpenGL 3.3 compatibility profile for legacy GL support
        std::array<EGLint, 7> contextAttribs = { EGL_CONTEXT_MAJOR_VERSION,
                                                 3,
                                                 EGL_CONTEXT_MINOR_VERSION,
                                                 3,
                                                 EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                                 EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
                                                 EGL_NONE };

        m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, contextAttribs.data());
        if (m_eglContext == EGL_NO_CONTEXT) {
            if constexpr (EGL_DEBUG) {
                std::cout << "[EglContext] Compatibility profile failed, trying default" << std::endl;
            }
            m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, EGL_NO_CONTEXT, nullptr);
        }

        if (m_eglContext == EGL_NO_CONTEXT) {
            std::cerr << "[EglContext] eglCreateContext failed: 0x" << std::hex << eglGetError() << std::dec
                      << std::endl;
            return false;
        }

        if constexpr (EGL_DEBUG) {
            std::cout << "[EglContext] Context created" << std::endl;
        }
        return true;
#else
        return false;
#endif
    }

    bool makeCurrent() override
    {
#ifdef __linux__
        if (m_eglDisplay == EGL_NO_DISPLAY || m_eglContext == EGL_NO_CONTEXT) {
            return false;
        }
        if (eglMakeCurrent(m_eglDisplay, m_eglSurface, m_eglSurface, m_eglContext) == 0U) {
            std::cerr << "[EglContext] eglMakeCurrent failed: 0x" << std::hex << eglGetError() << std::dec << std::endl;
            return false;
        }
        return true;
#else
        return false;
#endif
    }

    void swapBuffers() override
    {
#ifdef __linux__
        if (m_eglDisplay != EGL_NO_DISPLAY && m_eglSurface != EGL_NO_SURFACE) {
            eglSwapBuffers(m_eglDisplay, m_eglSurface);
        }
#endif
    }

    void release() override
    {
#ifdef __linux__
        if (m_eglDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
#endif
    }

    void cleanup() override
    {
#ifdef __linux__
        if (m_eglDisplay != EGL_NO_DISPLAY) {
            eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

            if (m_eglContext != EGL_NO_CONTEXT) {
                eglDestroyContext(m_eglDisplay, m_eglContext);
                m_eglContext = EGL_NO_CONTEXT;
            }
            if (m_eglSurface != EGL_NO_SURFACE) {
                eglDestroySurface(m_eglDisplay, m_eglSurface);
                m_eglSurface = EGL_NO_SURFACE;
            }
#ifdef HAVE_WAYLAND
            if (m_wlEglWindow) {
                wl_egl_window_destroy(m_wlEglWindow);
                m_wlEglWindow = nullptr;
            }
#endif
            // The EGLDisplay is shared by every context on the same X connection
            // (a popup reuses the main window's Display*, so eglGetPlatformDisplay
            // hands back the same handle). Terminating it from a per-window teardown
            // would invalidate all the other live contexts, so only the connection
            // owner (the main window; m_ownsDisplay) terminates it - once, here in
            // its own teardown, after its popups (children) are already gone and
            // before it closes the X connection. This prevents a dangling EGL
            // display from faulting a later eglCreateWindowSurface in the same
            // process (real-GPU drivers crash instead of erroring).
            if (m_ownsDisplay) {
                s_cachedValid   = false;
                s_cachedDisplay = EGL_NO_DISPLAY;
                s_cachedConfig  = nullptr;
                eglTerminate(m_eglDisplay);
            }
            m_eglDisplay = EGL_NO_DISPLAY;
        }
        m_initialized = false;
#endif
    }

    [[nodiscard]] bool hasAlpha() const override { return m_hasAlpha; }
    [[nodiscard]] bool hasMsaa() const override { return m_hasMsaa; }
    [[nodiscard]] bool isValid() const override
    {
#ifdef __linux__
        return m_initialized && m_eglContext != EGL_NO_CONTEXT;
#else
        return m_initialized;
#endif
    }

    // Additional accessors for platform-specific needs
#ifdef __linux__
    [[nodiscard]] EGLContext eglContext() const { return m_eglContext; }
    [[nodiscard]] EGLSurface eglSurface() const { return m_eglSurface; }
    /**
     * @brief Query actual EGL surface dimensions
     * @param width Output: surface width in pixels
     * @param height Output: surface height in pixels
     * @return true if query succeeded
     */
    bool querySurfaceSize(int & width, int & height) const
    {
        if (m_eglDisplay == EGL_NO_DISPLAY || m_eglSurface == EGL_NO_SURFACE) {
            width = height = 0;
            return false;
        }
        EGLint w = 0;
        EGLint h = 0;
        eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_WIDTH, &w);
        eglQuerySurface(m_eglDisplay, m_eglSurface, EGL_HEIGHT, &h);
        width  = w;
        height = h;
        return true;
    }
#endif

private:
    bool m_initialized = false;
    bool m_hasAlpha    = false;
    bool m_hasMsaa     = false;

#ifdef __linux__
    // Set by the connection owner (main window) via setOwnsDisplay(); only that
    // context terminates the shared EGLDisplay on teardown. Popups reuse the
    // parent's connection and leave this false.
    bool                            m_ownsDisplay   = false;
    EglPlatform                     m_platform      = EglPlatform::X11;
    Ui::Window::NativeDisplayHandle m_nativeDisplay = nullptr;
    EGLDisplay                      m_eglDisplay    = EGL_NO_DISPLAY;
    EGLContext                      m_eglContext    = EGL_NO_CONTEXT;
    EGLSurface                      m_eglSurface    = EGL_NO_SURFACE;
    EGLConfig                       m_eglConfig     = nullptr;

    // Process-wide cache of a discovered display+config, reused across sibling
    // contexts (e.g. each popup) to skip the slow eglInitialize + config search.
    inline static EGLDisplay s_cachedDisplay = EGL_NO_DISPLAY;
    inline static EGLConfig  s_cachedConfig  = nullptr;
    inline static bool       s_cachedMsaa    = false;
    inline static bool       s_cachedValid   = false;

#ifdef HAVE_WAYLAND
    wl_egl_window * m_wlEglWindow = nullptr;
#endif
#endif
};

} // namespace Ui::Window
