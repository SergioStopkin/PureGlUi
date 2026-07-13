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

// app.json keys - the single source of truth for their spelling, resolved
// through appKeyName().
enum class AppKey : unsigned char {
    Title,       // root: app title / openFile: dialog title
    SessionDir,  // root: session storage directory
    SessionFile, // root: session file name
    OpenFile,    // root: native open-dialog config object
    Filters,     // openFile: file-type filter list
    Name,        // filter entry: display name
    Spec,        // filter entry: pattern spec ("*.step;*.iges")
};

[[nodiscard]] inline std::string appKeyName(AppKey key)
{
    switch (key) {
    case AppKey::Title: return "title";
    case AppKey::SessionDir: return "sessionDir";
    case AppKey::SessionFile: return "sessionFile";
    case AppKey::OpenFile: return "openFile";
    case AppKey::Filters: return "filters";
    case AppKey::Name: return "name";
    case AppKey::Spec: return "spec";
    }
    return {};
}

} // namespace Ui::Res::Key
