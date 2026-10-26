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

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Ui::Res::Type {

// Dialog button actions
enum class DialogAction : uint8_t {
    None = 0,
    Ok,
    Cancel,
    Yes,
    No,
    Save,
    DontSave,
    Discard,
    CopyLink,
};

// The name map is built on the first call rather than before main, so a throw
// from its allocation reaches a caller instead of terminating the process
inline DialogAction dialogActionFromName(const std::string & name)
{
    // clang-format off
    static const std::unordered_map<std::string, DialogAction> NAMES = {
        { "Ok",       DialogAction::Ok },
        { "Cancel",   DialogAction::Cancel },
        { "Yes",      DialogAction::Yes },
        { "No",       DialogAction::No },
        { "Save",     DialogAction::Save },
        { "DontSave", DialogAction::DontSave },
        { "Discard",  DialogAction::Discard },
        { "CopyLink", DialogAction::CopyLink },
    };
    // clang-format on
    const auto it = NAMES.find(name);
    return (it != NAMES.end()) ? it->second : DialogAction::None;
}

} // namespace Ui::Res::Type
