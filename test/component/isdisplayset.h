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

// Whether the run is told a display to open: DISPLAY in an X11 build, WAYLAND_DISPLAY in a native
// Wayland one, none elsewhere. Told one, a window that cannot be made there fails the test: a skip
// would pass a run that tested nothing. Shared with the host's tests, so the failure says it here

#include "common/system.h"

namespace PureGlUi {

constexpr const char * DISPLAY_UNREACHABLE = "a display is set here, yet no window or GL context could be made";

[[nodiscard]] inline bool isDisplaySet()
{
#ifdef HAVE_WAYLAND
    return !Common::System::environmentVariable("WAYLAND_DISPLAY").empty();
#elifdef HAVE_X11
    return !Common::System::environmentVariable("DISPLAY").empty();
#else
    return false;
#endif
}

} // namespace PureGlUi
