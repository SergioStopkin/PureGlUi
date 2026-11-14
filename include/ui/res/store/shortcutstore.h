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
#include "ui/intent.h"
#include "ui/intentkind.h"
#include "ui/res/type/changed.h"
#include "ui/res/util.h"

#include <string>
#include <unordered_map>

namespace Ui::Res::Store {

// Loads and holds res/shortcut.json: normalized key-combo -> the action it runs,
// with its optional "arg". One of the logical sub-stores ResManager composes.
// load() returns Changed::Shortcut when the map actually changed (else None) so
// the facade can aggregate it.
class ShortcutStore final {
    std::unordered_map<std::string, Ui::intent_t> m_shortcuts; // key combo -> EmitAction intent

    // One entry, from either an array element or a lone object. A missing or
    // mistyped field reads as empty, and an entry without both "keys" and
    // "action" is not a shortcut, so it is skipped rather than binding "" to something.
    void addShortcut(const nlohmann::json & entry)
    {
        const std::string keys   = Common::Sanitize::string(Common::Json::string(entry, "keys"), "shortcut.keys");
        const std::string action = Common::Sanitize::string(Common::Json::string(entry, "action"), "shortcut.action");
        if (keys.empty() || action.empty()) {
            return;
        }
        m_shortcuts[Util::strKey(keys)] = {
            .kind      = Ui::IntentKind::EmitAction,
            .actionKey = action,
            .arg       = Common::Sanitize::string(Common::Json::string(entry, "arg"), "shortcut.arg"),
        };
    }

public:
    [[nodiscard]] const std::unordered_map<std::string, Ui::intent_t> & shortcuts() const { return m_shortcuts; }

    [[nodiscard]] Type::Changed load(const std::string & file)
    {
        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return Type::Changed::None;
        }

        auto oldShortcuts = m_shortcuts;
        m_shortcuts.clear();
        if (j.is_array()) {
            for (const auto & item : j) {
                addShortcut(item);
            }
        } else {
            addShortcut(j);
        }

        return m_shortcuts != oldShortcuts ? Type::Changed::Shortcut : Type::Changed::None;
    }
};

} // namespace Ui::Res::Store
