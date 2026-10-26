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

namespace Common {

// A buffer as the bytes a C API takes for data of another type (XChangeProperty's
// format-32 longs, librsvg's text), passed as c.data() - never the container itself.
// It arrives as void* by plain conversion, and reading it as bytes is defined behavior
[[nodiscard]] inline const unsigned char * asBytes(const void * buffer)
{
    return static_cast<const unsigned char *>(buffer);
}

} // namespace Common
