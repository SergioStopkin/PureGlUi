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

#include <cstdint>

namespace Ui::Io::Platform {

// How a portal request ended: the code of its Request.Response signal
enum class PortalResponse : std::uint8_t {
    Succeeded = 0, // the user picked; not Success, which X11's X.h defines as a macro
    Cancelled = 1, // the user closed it with nothing picked
    Ended     = 2, // neither: closed by Esc or its close button, or its backend failed to show it
};

} // namespace Ui::Io::Platform
