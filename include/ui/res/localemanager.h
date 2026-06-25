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
#include "ui/res/respath.h"

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

namespace Ui::Res {

class LocaleManager final {
    std::unordered_map<std::string, std::string> m_strings;

public:
    LocaleManager() = default;

    // Load locale strings from res/locale/{locale}.json
    bool load(const std::string & locale, const Ui::Res::ResPath & res)
    {
        const std::string path = res.locale(locale);
        std::ifstream     file(path);
        if (!file.is_open()) {
            std::cerr << "[LocaleManager] Cannot open locale file: " << path << std::endl;
            return false;
        }

        nlohmann::json j;
        try {
            file >> j;
        } catch (const std::exception & e) {
            std::cerr << "[LocaleManager] JSON parse error in " << path << ": " << e.what() << std::endl;
            return false;
        }

        int loaded = 0;
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!it.value().is_string()) {
                continue;
            }
            m_strings[it.key()] = Common::Sanitize::string(it.value().get<std::string>(), it.key());
            ++loaded;
        }

        std::cout << "[LocaleManager] Loaded " << loaded << " strings from " << path << std::endl;
        return true;
    }

    // Register a dynamic string (not from locale file)
    void set(const std::string & key, const std::string & value) { m_strings[key] = value; }

    // Lookup by string key
    [[nodiscard]] const std::string & get(const std::string & key) const
    {
        static const std::string empty;
        auto                     it = m_strings.find(key);
        if (it != m_strings.end()) {
            return it->second;
        }
        return empty;
    }
};

} // namespace Ui::Res
