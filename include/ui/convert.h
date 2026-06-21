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

#include "ui/res/type/border.h"
#include "ui/type.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace Ui {

class Convert final {
public:
    Convert()  = delete;
    ~Convert() = delete;

    Convert(const Convert &)             = delete;
    Convert & operator=(const Convert &) = delete;

    // Convert CSS-like string to int ("48px", "66%", "100vw", "0", etc.)
    static int str2int(const std::string & val, int base = 0, int ref = 0)
    {
        std::string s = val;
        // Remove whitespace
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        if (s.empty()) {
            return 0;
        }

        // Handle px
        if (s.size() > 2 && s.substr(s.size() - 2) == "px") {
            return std::stoi(s.substr(0, s.size() - 2));
        }
        // Handle %
        if (s.back() == '%') {
            const int percent = std::stoi(s.substr(0, s.size() - 1));
            return ref > 0 ? (percent * ref) / 100 : percent; // If ref is 0, just return percent
        }
        // Handle vw (viewport width, fallback to ref if provided)
        if (s.size() > 2 && s.substr(s.size() - 2) == "vw") {
            const int vw = std::stoi(s.substr(0, s.size() - 2));
            return ref > 0 ? (vw * ref) / 100 : vw; // 100vw = ref
        }
        // Handle vh (viewport height, fallback to ref if provided)
        if (s.size() > 2 && s.substr(s.size() - 2) == "vh") {
            const int vh = std::stoi(s.substr(0, s.size() - 2));
            return ref > 0 ? (vh * ref) / 100 : vh;
        }
        // Handle plain integer
        try {
            return std::stoi(s, nullptr, base);
        } catch (...) {
            return 0;
        }
    }

    // Convert CSS-like string to fpx_t (delegates to str2int)
    static fpx_t str2fpx(const std::string & val, int base = 0, int ref = 0)
    {
        return static_cast<fpx_t>(str2int(val, base, ref));
    }

    static std::uint32_t str2uint32(const std::string & val, int base = 0, std::uint32_t ref = 0)
    {
        std::string s = val;
        s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
        if (s.empty()) {
            return 0;
        }

        // px
        if (s.size() > 2 && s.substr(s.size() - 2) == "px") {
            return static_cast<std::uint32_t>(std::stoul(s.substr(0, s.size() - 2)));
        }
        // %
        if (s.back() == '%') {
            const auto percent = static_cast<std::uint32_t>(std::stoul(s.substr(0, s.size() - 1)));
            return ref > 0 ? (percent * ref) / 100 : percent;
        }
        // vw
        if (s.size() > 2 && s.substr(s.size() - 2) == "vw") {
            const auto vw = static_cast<std::uint32_t>(std::stoul(s.substr(0, s.size() - 2)));
            return ref > 0 ? (vw * ref) / 100 : vw;
        }
        // vh
        if (s.size() > 2 && s.substr(s.size() - 2) == "vh") {
            const auto vh = static_cast<std::uint32_t>(std::stoul(s.substr(0, s.size() - 2)));
            return ref > 0 ? (vh * ref) / 100 : vh;
        }
        // plain integer
        try {
            return static_cast<std::uint32_t>(std::stoul(s, nullptr, base));
        } catch (...) {
            return 0;
        }
    }

    // Parse a plain CSS number with no unit handling (e.g. "0.15", "14",
    // "1.4"). Unlike str2fpx this does not strip px/%/vw/vh - use it for
    // unitless values (opacity, line-height, raw pixel counts).
    static float parseCssNumber(const std::string & css)
    {
        try {
            return std::stof(css);
        } catch (...) {
            return 0.0F;
        }
    }

    // Parse an integer value from a CSS string (e.g. "14px" -> 14, "400" -> 400).
    static int parseCssInt(const std::string & css)
    {
        try {
            return std::stoi(css);
        } catch (...) {
            return 0;
        }
    }

    // Parse a CSS border-radius shorthand into border_t.
    //   1 value:  all corners
    //   2 values: top-left+bottom-right, top-right+bottom-left
    //   3 values: top-left, top-right+bottom-left, bottom-right
    //   4 values: top-left, top-right, bottom-right, bottom-left
    static Res::Type::border_t parseCssBorderRadius(const std::string & css)
    {
        std::vector<fpx_t> vals;
        std::istringstream stream(css);
        std::string        token;
        while (stream >> token) {
            vals.emplace_back(parseCssNumber(token));
        }
        if (vals.empty()) {
            return {};
        }
        if (vals.size() == 1) {
            return { vals[0], vals[0], vals[0], vals[0] };
        }
        if (vals.size() == 2) {
            return { vals[0], vals[1], vals[0], vals[1] };
        }
        if (vals.size() == 3) {
            return { vals[0], vals[1], vals[2], vals[1] };
        }
        return { vals[0], vals[1], vals[2], vals[3] };
    }
};

} // namespace Ui
