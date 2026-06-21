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

#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace Ui::Res {

// Resolves bundled resource paths relative to the res/ base directory.
// Joins via fs::path and returns forward-slash strings via generic_string()
// so log output, SVG cache keys, and content surface font registration stay
// platform-neutral (no mixed / and \ on Windows).
class ResPath final {
    std::filesystem::path m_base;

    [[nodiscard]] std::string join(std::string_view sub) const { return (m_base / sub).generic_string(); }

public:
    explicit ResPath(std::string base = "./res")
        : m_base(std::move(base))
    {
    }

    [[nodiscard]] std::string icon(std::string_view name) const { return join("icon/") + std::string(name); }
    [[nodiscard]] std::string fontDir() const { return join("font"); }
    [[nodiscard]] std::string fontFile(std::string_view name) const { return join("font/") + std::string(name); }
    [[nodiscard]] std::string theme(std::string_view name, std::string_view mode) const
    {
        return submenuDir("theme") + "/" + std::string(name) + "-" + std::string(mode) + ".json";
    }
    [[nodiscard]] std::string layoutFile() const { return join("css/layout.json"); }
    [[nodiscard]] std::string appFile() const { return join("app.json"); }
    [[nodiscard]] std::string renderFile() const { return join("render.json"); }
    [[nodiscard]] std::string inputFile() const { return join("input.json"); }
    [[nodiscard]] std::string dialogFile() const { return join("dialog.json"); }
    [[nodiscard]] std::string iconDefaultsFile() const { return join("icon-defaults.json"); }
    [[nodiscard]] std::string shortcutFile() const { return join("shortcut.json"); }
    // res/submenu/ is the parent for every per-file resource directory that
    // menu items can auto-populate from. ResManager walks submenuRoot() to
    // discover the available source names (each child directory is one
    // source key), and submenuDir(name) opens a specific one.
    [[nodiscard]] std::string submenuRoot() const { return join("submenu"); }
    [[nodiscard]] std::string submenuDir(std::string_view name) const { return join("submenu/") + std::string(name); }
    [[nodiscard]] std::string buttonDir() const { return join("button"); }
    [[nodiscard]] std::string menuDir() const { return join("menu"); }
    [[nodiscard]] std::string dockDir() const { return join("dock"); }
    [[nodiscard]] std::string locale(std::string_view lang) const
    {
        return join("locale/") + std::string(lang) + ".json";
    }
};

} // namespace Ui::Res
