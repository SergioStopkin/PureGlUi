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

#include "ui/backend/window/nativedisplayhandle.h"
#include "ui/backend/window/nativewindowhandle.h"

#include <cstdint>

namespace Ui::Backend::Window {

/**
 * @brief Interface for graphics context (OpenGL via EGL/GLX/WGL)
 *
 * Abstracts platform-specific OpenGL context creation and management.
 * Self-rooted (own virtual dtor).
 */
class IContext {
public:
    virtual ~IContext() = default;

    /**
     * @brief Initialize the context for a display
     * @param display Platform display handle (X11 Display*, etc.)
     * @return true on success
     */
    virtual bool init(Ui::Backend::Window::NativeDisplayHandle display) = 0;

    /**
     * @brief Choose a suitable config/visual
     * @param wantAlpha Request alpha channel for transparency
     * @param wantMsaa Request multisampling
     * @return true on success
     */
    virtual bool chooseConfig(bool wantAlpha, bool wantMsaa) = 0;

    /**
     * @brief Get native visual ID for window creation
     * @return Visual ID (X11 VisualID, etc.)
     */
    [[nodiscard]] virtual uint64_t visualId() const = 0;

    /**
     * @brief Create surface for a window
     * @param window Native window handle (X11 Window XID or pointer-as-integer)
     * @return true on success
     */
    virtual bool createSurface(Ui::Backend::Window::NativeWindowHandle window) = 0;

    /**
     * @brief Create the OpenGL context
     * @return true on success
     */
    virtual bool createContext() = 0;

    /**
     * @brief Make this context current for rendering
     * @return true on success
     */
    virtual bool makeCurrent() = 0;

    /**
     * @brief Swap front/back buffers
     */
    virtual void swapBuffers() = 0;

    /**
     * @brief Release context (make none current)
     */
    virtual void release() = 0;

    /**
     * @brief Clean up all resources
     */
    virtual void cleanup() = 0;

    /**
     * @brief Check if context has alpha channel
     */
    [[nodiscard]] virtual bool hasAlpha() const = 0;

    /**
     * @brief Check if context has MSAA
     */
    [[nodiscard]] virtual bool hasMsaa() const = 0;

    /**
     * @brief Check if context is valid and ready
     */
    [[nodiscard]] virtual bool isValid() const = 0;
};

} // namespace Ui::Backend::Window
