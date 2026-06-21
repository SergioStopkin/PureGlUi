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

#include <filesystem>
#include <iostream>
#include <string>

namespace Common {

// True if dir exists and is a directory; logs + returns false otherwise.
// Guards directory_iterator loops, which throw on a missing path (unlike the
// file loaders, which fail gracefully via Common::loadJson's open check).
inline bool dirExists(const std::string & dir)
{
    if (!std::filesystem::exists(dir) || !std::filesystem::is_directory(dir)) {
        std::cerr << "Directory does not exist or is not a directory: " << dir << std::endl;
        return false;
    }
    return true;
}

} // namespace Common
