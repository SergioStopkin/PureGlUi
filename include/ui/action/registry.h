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

#include "ui/type.h"

#include <string>
#include <unordered_map>
#include <utility>

namespace Ui::Action {

// Action-dispatch table: actionKey -> handler(arg). The fw owns the mechanism;
// the host fills it with handlers. Handlers are opaque action_fn_t, so this
// stays domain-blind (it never knows what an action does). One entry per action;
// parameterized actions read arg, simple ones ignore it. Dispatch is host-driven:
// the host calls dispatch with the actionKey surfaced by a UI click/shortcut.
class Registry final {
    std::unordered_map<std::string, Ui::action_fn_t> m_handlers;

public:
    void on(std::string actionKey, Ui::action_fn_t handler) { m_handlers[std::move(actionKey)] = std::move(handler); }

    void dispatch(const std::string & actionKey, const std::string & arg = "") const
    {
        if (const auto it = m_handlers.find(actionKey); it != m_handlers.end()) {
            it->second(arg);
        }
    }

    [[nodiscard]] bool has(const std::string & actionKey) const
    {
        return m_handlers.find(actionKey) != m_handlers.end();
    }
};

} // namespace Ui::Action
