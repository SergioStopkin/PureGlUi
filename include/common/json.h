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

#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <string>

namespace Common {

// Parse a JSON file into j. Returns false (and logs) on open/parse failure.
// Domain-blind file utility shared by the UI resource loaders (ResManager)
// and the host scene-config loaders (SceneSettings).
inline bool loadJson(const std::string & file, nlohmann::json & j)
{
    std::ifstream f(file);
    if (!f) {
        std::cerr << "Cannot open file [" << file << "]" << std::endl;
        return false;
    }

    try {
        f >> j;
    } catch (const std::exception & e) {
        std::cerr << "JSON parse error in file [" << file << "]: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Unknown JSON parse error in file [" << file << "]" << std::endl;
        return false;
    }

    return true;
}

} // namespace Common
