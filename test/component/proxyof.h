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

// What the compositor sends, fed by hand to a protocol object's own listener: where no
// compositor here sends it, or not when a test needs it

#ifdef HAVE_WAYLAND

#include <wayland-client.h>

namespace PureGlUi {

// The proxy a protocol object is, as libwayland's generated code takes it
inline wl_proxy * proxyOf(void * object) { return static_cast<wl_proxy *>(object); }

} // namespace PureGlUi

#endif
