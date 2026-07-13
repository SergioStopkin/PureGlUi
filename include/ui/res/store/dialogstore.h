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
#include "ui/res/key/dialog.h"
#include "ui/res/type/dialog.h"

#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>

namespace Ui::Res::Store {

// Loads and holds res/dialog.json: per dialog-type button rows (label + action
// + primary flag), resolved from the shared button-label table. One of the
// logical sub-stores ResManager composes.
class DialogStore final {
    std::unordered_map<Type::DialogType, Type::dialog_type_config_t> m_dialogTypeConfigs;

public:
    [[nodiscard]] const Type::dialog_type_config_t & dialogTypeConfig(Type::DialogType type) const
    {
        static const Type::dialog_type_config_t empty;
        auto                                    it = m_dialogTypeConfigs.find(type);
        return (it != m_dialogTypeConfigs.end()) ? it->second : empty;
    }

    void load(const std::string & file)
    {
        using Ui::Res::Key::DialogKey;
        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return;
        }

        const std::string buttonsKey = dialogKeyName(DialogKey::Buttons);
        const std::string labelKey   = dialogKeyName(DialogKey::Label);

        // Parse button definitions: name -> locale label
        std::unordered_map<std::string, std::string> buttonLabels;
        if (j.contains(buttonsKey) && j[buttonsKey].is_object()) {
            for (auto it = j[buttonsKey].begin(); it != j[buttonsKey].end(); ++it) {
                if (it.value().is_object() && it.value().contains(labelKey)) {
                    buttonLabels[it.key()] = Common::Sanitize::string(it.value()[labelKey].get<std::string>(),
                                                                      "dialog.button.label");
                }
            }
        }

        // Parse type definitions: type name -> button list + primary
        const std::string typesKey = dialogKeyName(DialogKey::Types);
        if (j.contains(typesKey) && j[typesKey].is_object()) {
            for (auto it = j[typesKey].begin(); it != j[typesKey].end(); ++it) {
                const Type::DialogType type    = Type::dialogTypeFromName(it.key());
                const auto &           typeDef = it.value();
                if (!typeDef.is_object() || !typeDef.contains(buttonsKey) || !typeDef[buttonsKey].is_array()) {
                    continue;
                }

                const std::string primaryName = Common::Sanitize::string(
                typeDef.value(dialogKeyName(DialogKey::Primary), ""),
                "dialog.primary");
                Type::dialog_type_config_t config;

                for (const auto & btnName : typeDef[buttonsKey]) {
                    if (!btnName.is_string()) {
                        continue;
                    }
                    const std::string name    = btnName.get<std::string>();
                    auto              labelIt = buttonLabels.find(name);
                    if (labelIt == buttonLabels.end()) {
                        std::cerr << "[DialogStore] Unknown dialog button: " << name << std::endl;
                        continue;
                    }

                    Type::dialog_button_config_t button;
                    button.label   = labelIt->second;
                    button.action  = Type::dialogActionFromName(name);
                    button.primary = (name == primaryName);
                    config.buttons.emplace_back(button);
                }

                m_dialogTypeConfigs[type] = std::move(config);
            }
        }
    }
};

} // namespace Ui::Res::Store
