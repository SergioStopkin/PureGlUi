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

#include "ui/backend/gl/localglew.h"

#include <cstdint>
#include <vector>

namespace Ui::Window {

// Offscreen surface captured to a texture for main-window compositing
// (Wayland XComposite path renders a surface to a texture and blits it).
struct CompositeTexture final {
    GLuint               texture = 0;
    std::vector<uint8_t> pixels;
    int                  w     = 0;
    int                  h     = 0;
    bool                 dirty = false;

    void capture(int width, int height)
    {
        if (width <= 0 || height <= 0) {
            return;
        }
        pixels.resize(static_cast<size_t>(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        w     = width;
        h     = height;
        dirty = true;
    }

    void clear()
    {
        pixels.clear();
        w     = 0;
        h     = 0;
        dirty = false;
    }

    void destroy()
    {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
            texture = 0;
        }
        clear();
    }
};

} // namespace Ui::Window
