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

// View a buffer as its raw bytes (unsigned char*) for C APIs that take byte
// pointers (XChangeProperty, librsvg, ...). Takes the pointer to the first
// element so a container is passed as c.data() - never the container object
// itself. char-family aliasing is defined behavior. Project rule: no
// reinterpret_cast - the two-step static_cast through void* is the same
// defined operation, and this named helper is its single home.
template <typename T>
[[nodiscard]] unsigned char * asBytes(T * buffer)
{
    return static_cast<unsigned char *>(static_cast<void *>(buffer));
}

template <typename T>
[[nodiscard]] const unsigned char * asBytes(const T * buffer)
{
    return static_cast<const unsigned char *>(static_cast<const void *>(buffer));
}

} // namespace Common
