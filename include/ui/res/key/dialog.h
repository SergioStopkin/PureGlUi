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

#include <string>

namespace Ui::Res::Key {

// res/dialog.json keys the DialogStore reads - the single source of truth for
// their spelling, resolved through dialogKeyName(). Defines the per dialog-type
// button rows; the inline dialog block on a menu item is a separate schema
// (see MenuKey). Button names and dialog-type names are opaque values keyed by
// the JSON object, not enumerated here.
enum class DialogKey : unsigned char {
    Buttons, // button-definition table (name -> {label}); also the per-type button list
    Label,   // a button definition's locale label
    Types,   // per dialog-type config table
    Primary, // the primary (default) button name within a type
};

[[nodiscard]] inline std::string dialogKeyName(DialogKey key)
{
    switch (key) {
    case DialogKey::Buttons: return "buttons";
    case DialogKey::Label: return "label";
    case DialogKey::Types: return "types";
    case DialogKey::Primary: return "primary";
    }
    return {};
}

} // namespace Ui::Res::Key
