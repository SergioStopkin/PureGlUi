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

#include "ui/intentkind.h"
#include "ui/type.h"

#include <string>

namespace Ui {

// One thing the framework asks the host to do (see IntentKind). A tagged record:
// `kind` selects which fields are meaningful. EmitAction uses actionKey + arg;
// OpenPopup/OpenSubmenu/OpenDialog/SwitchTab/CloseTab use id; CopyText uses arg;
// ClosePopup uses none. id-only intents ship no geometry - the host resolves the
// screen anchor via Context::bound(id).
struct alignas(128) intent_t final {
    IntentKind  kind = IntentKind::EmitAction;
    id_t        id   = INVALID_ID;
    key_t       actionKey;
    std::string arg;

    bool operator==(const intent_t &) const = default;
};

} // namespace Ui
