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

#include <cctype>
#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

namespace Ui::Res {

class Util final {
public:
    Util()  = delete;
    ~Util() = delete;

    Util(const Util &)             = delete;
    Util & operator=(const Util &) = delete;

    // Remove all spaces and convert to lowercase
    static std::string strKey(const std::string & str)
    {
        std::string result;
        result.reserve(str.size());
        for (const char ch : str) {
            if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
        }

        return result;
    }

    // Split a hierarchical menu key on ':' into its path segments
    // ("View:Theme:default" -> {"View","Theme","default"}). Always returns at
    // least one segment (an empty key yields a single empty segment).
    static std::vector<std::string> splitMenuKey(const std::string & key)
    {
        std::vector<std::string> segments;
        size_t                   start = 0;
        while (start <= key.size()) {
            const size_t pos = key.find(':', start);
            if (pos == std::string::npos) {
                segments.emplace_back(key.substr(start));
                break;
            }
            segments.emplace_back(key.substr(start, pos - start));
            start = pos + 1;
        }
        return segments;
    }

    // Generated key using ns time
    static std::size_t timeKey()
    {
        return static_cast<std::size_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count());
    }
};

} // namespace Ui::Res
