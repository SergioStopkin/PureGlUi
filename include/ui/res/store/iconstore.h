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

#include "common/json.h"
#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/res/key/icon.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/icondefault.h"
#include "ui/res/type/iconplace.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Ui::Res::Store {

// Loads and holds res/icon-defaults.json: per-role default icons (with the
// JSON's alias chain resolved to a concrete .svg) and their placement. One of
// the logical sub-stores ResManager composes.
class IconStore final {
    std::unordered_map<std::string, Type::icon_default_t> m_iconDefaults;

public:
    // Resolved icon default for a role. Returns an empty icon when the role is
    // unknown - callers treat that as "no marker".
    [[nodiscard]] Type::icon_default_t iconDefault(const std::string & role) const
    {
        auto it = m_iconDefaults.find(role);
        return (it != m_iconDefaults.end()) ? it->second : Type::icon_default_t {};
    }

    // Load res/icon-defaults.json with alias resolution. Each entry's `icon`
    // field is a real .svg filename (alias) or the name of another entry (role);
    // a role resolves with a single hop to its alias's .svg. `place` is each
    // entry's own value (default Left); it is not inherited across the hop.
    [[nodiscard]] Type::Changed load(const std::string & file)
    {
        nlohmann::json j;
        if (!Common::loadJson(file, j) || !j.is_object()) {
            return Type::Changed::None;
        }

        auto oldDefaults = m_iconDefaults;
        m_iconDefaults.clear();

        // Pass 1: parse every entry into m_iconDefaults (skip the optional
        // _comment). `icon` is either a real .svg (alias) or the name of another
        // entry (role); `place` defaults to Left when absent.
        const std::string iconKey  = Key::iconKeyName(Key::IconKey::Icon);
        const std::string placeKey = Key::iconKeyName(Key::IconKey::Place);
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key().rfind('_', 0) == 0 || !it.value().is_object()) {
                continue;
            }
            Type::icon_default_t entry;
            entry.icon = Common::Sanitize::filePath(it.value().value(iconKey, ""), "iconDefault.icon");
            if (it.value().contains(placeKey) && it.value()[placeKey].is_string()) {
                entry.place = Type::iconPlaceFromName(it.value()[placeKey].get<std::string>());
            }
            m_iconDefaults.emplace(it.key(), std::move(entry));
        }

        // Pass 2: resolve each role to its alias's .svg. Schema is two-tier -
        // aliases point at .svg, roles point at an alias - so a single hop
        // suffices; a role whose target is not a .svg resolves to empty.
        // Lookups go against the immutable pre-pass snapshot so a malformed
        // role->role chain resolves to empty deterministically, never leaking a
        // mid role's freshly resolved .svg (map iteration order must not matter).
        const std::string_view SVG_EXT     = ".svg";
        auto                   endsWithSvg = [&SVG_EXT](const std::string & s) {
            return s.size() >= SVG_EXT.size() && s.compare(s.size() - SVG_EXT.size(), SVG_EXT.size(), SVG_EXT) == 0;
        };
        const auto source = m_iconDefaults;
        for (auto & entryPair : m_iconDefaults) {
            auto & entry = entryPair.second;
            if (entry.icon.empty() || endsWithSvg(entry.icon)) {
                continue;
            }
            auto alias = source.find(entry.icon);
            entry.icon = (alias != source.end() && endsWithSvg(alias->second.icon)) ? alias->second.icon
                                                                                    : std::string {};
        }

        return m_iconDefaults != oldDefaults ? Type::Changed::Icon : Type::Changed::None;
    }
};

} // namespace Ui::Res::Store
