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

#include "common/sanitize.h"
#include "nlohmann/json.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

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

// Value-returning overload for the common "missing file means defaults" case:
// a failed load yields a null json, which every reader below then answers with
// its fallback. Use the two-argument form instead wherever a failed load must
// LEAVE existing values alone - the difference is defaults versus no-op.
[[nodiscard]] inline nlohmann::json loadJson(const std::string & file)
{
    nlohmann::json json;
    (void)loadJson(file, json);
    return json;
}

// Tolerant readers for res JSON, shared by the framework stores and the host
// loaders - both hand-rolled the same contains/is_*/get dance, and nlohmann's
// own value() is not a substitute: it THROWS when the key exists with the wrong
// type, which res JSON (hand-editable, a trust boundary) can always produce.
// Nothing here throws; a missing or wrongly-typed value is the fallback.
namespace Json {

    // Empty stand-ins so the container readers can return a reference and callers
    // can iterate the result unconditionally.
    [[nodiscard]] inline const nlohmann::json & emptyObject()
    {
        static const nlohmann::json empty = nlohmann::json::object();
        return empty;
    }

    [[nodiscard]] inline const nlohmann::json & emptyArray()
    {
        static const nlohmann::json empty = nlohmann::json::array();
        return empty;
    }

    [[nodiscard]] inline const nlohmann::json & object(const nlohmann::json & json, std::string_view key)
    {
        const auto it = json.is_object() ? json.find(key) : json.end();
        return (it != json.end() && it->is_object()) ? *it : emptyObject();
    }

    [[nodiscard]] inline const nlohmann::json & array(const nlohmann::json & json, std::string_view key)
    {
        const auto it = json.is_object() ? json.find(key) : json.end();
        return (it != json.end() && it->is_array()) ? *it : emptyArray();
    }

    [[nodiscard]] inline std::string string(const nlohmann::json & json,
                                            std::string_view       key,
                                            const std::string &    fallback = {})
    {
        const auto it = json.is_object() ? json.find(key) : json.end();
        return (it != json.end() && it->is_string()) ? it->get<std::string>() : fallback;
    }

    // The res trust-boundary rule in one place: every string ingested from res JSON
    // is sanitized at parse, and the key doubles as the log label.
    [[nodiscard]] inline std::string sanitizedString(const nlohmann::json & json,
                                                     std::string_view       key,
                                                     const std::string &    fallback = {})
    {
        return Sanitize::string(string(json, key, fallback), key);
    }

    [[nodiscard]] inline bool boolean(const nlohmann::json & json, std::string_view key, bool fallback = false)
    {
        const auto it = json.is_object() ? json.find(key) : json.end();
        return (it != json.end() && it->is_boolean()) ? it->get<bool>() : fallback;
    }

    // Unsigned targets additionally require an unsigned JSON number, so a negative
    // literal falls back instead of wrapping into a huge value.
    template <typename Number>
        requires std::is_arithmetic_v<Number>
    [[nodiscard]] inline Number number(const nlohmann::json & json, std::string_view key, Number fallback = {})
    {
        const auto it = json.is_object() ? json.find(key) : json.end();
        if (it == json.end() || !it->is_number()) {
            return fallback;
        }
        if constexpr (std::is_unsigned_v<Number>) {
            if (!it->is_number_unsigned()) {
                return fallback;
            }
        }
        return it->get<Number>();
    }

    // A fixed-length numeric vector ("eye": [x, y, z]) - nullopt unless the value is
    // an array of exactly Size numbers, so a short or mistyped one never yields a
    // half-filled result.
    template <std::size_t Size>
    [[nodiscard]] inline std::optional<std::array<double, Size>> numbers(const nlohmann::json & value)
    {
        if (!value.is_array() || value.size() != Size) {
            return std::nullopt;
        }
        std::array<double, Size> result {};
        std::size_t              index = 0;
        for (const auto & component : value) {
            if (!component.is_number()) {
                return std::nullopt;
            }
            result[index] = component.get<double>();
            ++index;
        }
        return result;
    }

} // namespace Json

} // namespace Common
