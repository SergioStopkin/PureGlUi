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

// All files that need OpenGL should include this header instead of <GL/glew.h> or <GL/gl.h>.
// On macOS, OpenGL functions are available directly via <OpenGL/gl3.h> without an extension loader.
// On Linux/Windows, GLEW is used as the extension loader and must be included before any other GL headers.

#ifdef __APPLE__
#include <OpenGL/gl3.h>
// gl3.h caps at the newest GL macOS offers, so a consumer that re-declares the
// API from its own glext must top it up rather than re-emit it. Those headers
// gate an undef-all block on GL_VERSION_1_2; clearing it keeps every version
// gl3.h already declared intact, and the glext still adds the ones above it.
#undef GL_VERSION_1_2
#else
#include <GL/glew.h>
#endif
