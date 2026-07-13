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

// session.json keys the FRAMEWORK writes (format v2) - the single source of
// truth for their spelling, resolved through sessionKeyName(). Host-registered
// persisted settings keep free-form string keys (the registry stays open for
// domain keys the fw cannot enumerate).
enum class SessionKey : unsigned char {
    Version,     // root: app version (CMake PROJECT_VERSION)
    LastUpdate,  // root: epoch milliseconds of the last save
    X,           // mainWindow: frame origin, physical px
    Y,           // mainWindow: frame origin, physical px
    Width,       // mainWindow client size / dock committed width
    Height,      // mainWindow client size
    Theme,       // view: theme name
    ThemeMode,   // view: dark/light
    LastOpenDir, // paths: last open-dialog directory
    Active,      // files: the active file path
    Open,        // files: all open file paths
    Name,        // docks entry: dock identity (res/dock JSON "name")
    MemoryX,     // docks entry: width remembered for double-click restore
};

[[nodiscard]] inline std::string sessionKeyName(SessionKey key)
{
    switch (key) {
    case SessionKey::Version: return "version";
    case SessionKey::LastUpdate: return "lastUpdate";
    case SessionKey::X: return "x";
    case SessionKey::Y: return "y";
    case SessionKey::Width: return "width";
    case SessionKey::Height: return "height";
    case SessionKey::Theme: return "theme";
    case SessionKey::ThemeMode: return "themeMode";
    case SessionKey::LastOpenDir: return "lastOpenDir";
    case SessionKey::Active: return "active";
    case SessionKey::Open: return "open";
    case SessionKey::Name: return "name";
    case SessionKey::MemoryX: return "memoryX";
    }
    return {};
}

} // namespace Ui::Res::Key
