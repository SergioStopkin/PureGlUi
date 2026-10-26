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

namespace Ui::Res {

// One persisted user setting, framework's or host's: the session is saved and restored
// by walking these instead of naming every field
struct alignas(128) persisted_setting_t final {
    std::string       section;             // resolved session.json section name (fw or host enum)
    std::string       sessionKey;          // camelCase key inside the section (empty when isJsonValue)
    Ui::provider_fn_t get;                 // current value -> string
    Ui::action_fn_t   set;                 // string -> apply to typed field
    bool              skipIfEmpty = false; // omit from save when value is empty
    bool              isJsonValue = false; // value is JSON text and owns the whole section
};

} // namespace Ui::Res
