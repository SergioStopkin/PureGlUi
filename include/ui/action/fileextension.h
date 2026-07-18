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

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace Ui::Action {

// Lowercase file extension without the dot ("/p/Model.STEP" -> "step"). The
// extension-dispatch key for OpenFile handlers. Pure std::filesystem, kept free
// of the native file-dialog header (which drags Cocoa on macOS) so it is usable
// from the unit-test tier.
inline std::string fileExtension(const std::string & file)
{
    std::string extension = std::filesystem::path(file).extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(0, 1);
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

} // namespace Ui::Action
