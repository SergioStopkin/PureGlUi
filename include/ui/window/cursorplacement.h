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

namespace Ui::Window {

// A cursor image as its Wayland surface shows it
struct alignas(16) cursor_placement_t final {
    int bufferScale = 1; // image px per surface px, as wl_surface_set_buffer_scale takes it
    int hotspotX    = 0; // surface px from the image's left, as wl_pointer_set_cursor takes it
    int hotspotY    = 0; // surface px from the image's top
};

} // namespace Ui::Window
