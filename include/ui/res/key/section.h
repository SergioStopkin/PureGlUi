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

// session.json top-level sections (format v2) - the single source of truth
// for the file's object names. Everything that reads or writes a session
// section resolves its key through sectionKeyName(), so a section can never
// be spelled two ways.
enum class SectionKey : unsigned char {
    MainWindow, // main-window geometry (frame x/y + client width/height, physical px)
    View,       // appearance: theme, themeMode
    Paths,      // path-like settings: lastOpenDir, ...
    Files,      // open files: active, open[] - file paths, not tabs/workspaces
                // (tabs are domain-blind chrome, workspaces are host domain;
                // what the fw persists is exactly the file list)
    Docks,      // per-dock committed state (array; plural like every collection section)
};

[[nodiscard]] inline std::string sectionKeyName(SectionKey section)
{
    switch (section) {
    case SectionKey::MainWindow: return "mainWindow";
    case SectionKey::View: return "view";
    case SectionKey::Paths: return "paths";
    case SectionKey::Files: return "files";
    case SectionKey::Docks: return "docks";
    }
    return {};
}

} // namespace Ui::Res::Key
