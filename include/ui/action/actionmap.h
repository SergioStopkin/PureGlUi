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

#include "ui/action/exitapp.h"
#include "ui/action/openfile.h"
#include "ui/action/reload.h"
#include "ui/action/switchtheme.h"
#include "ui/action/switchthememode.h"
#include "ui/type.h"

#include <array>
#include <string>
#include <utility>

namespace Ui::Action {

// Bind every built-in framework action into the host's Action::Registry, pairing
// each "action" key (as authored in res JSON) with its implementation. A host
// adds its domain actions the same way via host.actions().on(...); keys with no
// binding are greyed out by the disable pass.
//
// The list is a plain iterate-once array, NOT a map: the live key->handler map
// is the Registry this seeds (and which the host extends with domain actions),
// so a second hash map here would just duplicate it. Templated on the host so
// Ui::Action never depends on the concrete shell type.
template <typename Host>
void registerActions(Host & host)
{
    using Fn           = void (*)(Host &, const std::string &);
    const auto actions = std::to_array<std::pair<Ui::key_t, Fn>>({
    { "ExitApp", &exitApp<Host> },
    { "Reload", &reload<Host> },
    { "SwitchThemeMode", &switchThemeMode<Host> },
    { "SwitchTheme", &switchTheme<Host> },
    { "OpenFile", &openFile<Host> },
    });
    for (const auto & [key, fn] : actions) {
        host.actions().on(key, [&host, fn](const std::string & arg) { fn(host, arg); });
    }
}

} // namespace Ui::Action
