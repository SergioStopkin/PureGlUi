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

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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

// clang-format off
// NOLINTNEXTLINE(cert-err58-cpp)
inline const std::unordered_map<std::string, DialogAction> nameToDialogAction = {
    { "Ok",       DialogAction::Ok },
    { "Cancel",   DialogAction::Cancel },
    { "Yes",      DialogAction::Yes },
    { "No",       DialogAction::No },
    { "Save",     DialogAction::Save },
    { "DontSave", DialogAction::DontSave },
    { "Discard",  DialogAction::Discard },
    { "CopyLink", DialogAction::CopyLink },
};

// Dialog window types
enum class DialogType : uint8_t {
    Info,        // [OK] - simple information display
    InfoLink,    // [Copy Link] [OK] - information with copyable link
    Confirm,     // [Yes] [No] - confirmation prompt
    SaveConfirm, // [Don't Save] [Cancel] [Save] - unsaved changes prompt
    Warning,     // [OK] - warning (icon tinted with --cl-warn)
};

// NOLINTNEXTLINE(cert-err58-cpp)
inline const std::unordered_map<std::string, DialogType> nameToDialogType = {
    { "Info",        DialogType::Info },
    { "InfoLink",    DialogType::InfoLink },
    { "Confirm",     DialogType::Confirm },
    { "SaveConfirm", DialogType::SaveConfirm },
    { "Warning",     DialogType::Warning },
};
// clang-format on

inline DialogAction dialogActionFromName(const std::string & name)
{
    auto it = nameToDialogAction.find(name);
    return (it != nameToDialogAction.end()) ? it->second : DialogAction::None;
}

inline DialogType dialogTypeFromName(const std::string & name)
{
    auto it = nameToDialogType.find(name);
    return (it != nameToDialogType.end()) ? it->second : DialogType::Info;
}

// Button config loaded from res/dialog.json
struct alignas(64) dialog_button_config_t final {
    std::string  label; // locale key for button text
    DialogAction action  = DialogAction::None;
    bool         primary = false;
};

// Type config loaded from res/dialog.json
struct alignas(128) dialog_type_config_t final {
    std::vector<dialog_button_config_t> buttons;
};

// Dialog definition - all text comes from locale
struct alignas(128) dialog_t final {
    DialogType   type = DialogType::Info;
    std::string  title;           // locale key for dialog title
    std::string  content;         // locale key for dialog body text
    std::string  link;            // locale key for URL (InfoLink dialogs)
    std::string  icon;            // SVG icon filename (e.g. "2139.svg")
    std::string  file;            // file path for content (overrides locale content)
    std::wstring contentOverride; // runtime content (resolved at open time)
    fpx_t        width {};        // per-dialog override (0 = use layout default)
    fpx_t        height {};

    bool operator==(const dialog_t &) const = default;
};

} // namespace Ui::Res::Type
