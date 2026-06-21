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
#include "ui/res/type/changed.h"
#include "ui/res/type/icondefault.h"
#include "ui/res/type/iconplace.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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
    // field can be a real .svg filename or the name of another entry; we follow
    // the chain until a .svg is hit. `place` is the first explicit value walking
    // leaf to root, defaulting to Left.
    [[nodiscard]] Type::Changed load(const std::string & file)
    {
        nlohmann::json j;
        if (!Common::loadJson(file, j) || !j.is_object()) {
            return Type::Changed::None;
        }

        auto oldDefaults = m_iconDefaults;
        m_iconDefaults.clear();

        // Pass 1: stash raw entries (skip the optional _comment).
        struct raw_t {
            std::string                    icon;
            std::optional<Type::IconPlace> place;
        };
        std::unordered_map<std::string, raw_t> raw;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key().rfind('_', 0) == 0 || !it.value().is_object()) {
                continue;
            }
            raw_t entry;
            entry.icon = Common::Sanitize::filePath(it.value().value("icon", ""), "iconDefault.icon");
            if (it.value().contains("place") && it.value()["place"].is_string()) {
                entry.place = Type::iconPlaceFromName(it.value()["place"].get<std::string>());
            }
            raw.emplace(it.key(), std::move(entry));
        }

        // Pass 2: resolve each entry with a single alias lookup. Schema is two-tier
        // by convention - aliases point at .svg, roles point at an alias - so one
        // hop is always enough. If `icon` is not a .svg, look it up once; if that
        // entry's icon is also not a .svg, the role resolves to an empty icon.
        const std::string_view SVG_EXT     = ".svg";
        auto                   endsWithSvg = [&](const std::string & s) {
            return s.size() >= SVG_EXT.size() && s.compare(s.size() - SVG_EXT.size(), SVG_EXT.size(), SVG_EXT) == 0;
        };
        for (const auto & [name, entry] : raw) {
            std::string                    ic    = entry.icon;
            std::optional<Type::IconPlace> place = entry.place;
            if (!ic.empty() && !endsWithSvg(ic)) {
                auto alias = raw.find(ic);
                if (alias != raw.end() && endsWithSvg(alias->second.icon)) {
                    ic = alias->second.icon;
                    if (!place.has_value()) {
                        place = alias->second.place;
                    }
                } else {
                    ic.clear();
                }
            }
            Type::icon_default_t resolved;
            resolved.icon  = ic;
            resolved.place = place.value_or(Type::IconPlace::Left);
            m_iconDefaults.emplace(name, std::move(resolved));
        }

        return m_iconDefaults != oldDefaults ? Type::Changed::Icon : Type::Changed::None;
    }
};

} // namespace Ui::Res::Store
