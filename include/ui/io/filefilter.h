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

#include <string>

namespace Ui::Io {

// One file-type filter for the native open dialog. Domain-blind: the app/host
// supplies the set (loaded from res JSON), so the framework hardcodes no
// extensions. `spec` is a canonical, semicolon-separated glob list
// ("*.step;*.stp;*.iges"); each platform backend reshapes it as needed
// (Win32 uses it verbatim, zenity wants spaces, macOS wants bare extensions).
struct alignas(64) file_filter_t final {
    std::string name; // human-readable, e.g. "CAD files (STEP, IGES)"
    std::string spec; // "*.step;*.stp;*.iges;*.igs"
};

} // namespace Ui::Io
