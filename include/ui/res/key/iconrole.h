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

// icon-defaults.json roles the FRAMEWORK looks up - the single source of
// truth for their spelling, resolved through iconRoleKeyName(). Roles that
// only other res JSON references (chevron, submenu, host-added roles) stay
// free-form strings: that set is deliberately open and data-driven.
enum class IconRoleKey : unsigned char {
    WindowIcon,         // main-window icon
    WindowIconSymbolic, // monochrome/symbolic main-window icon (optional)
    Warning,            // warning marker (no-handler dialog etc.)
    RowCollapsed,       // dock row expander, children hidden (optional)
    RowExpanded,        // dock row expander, children shown (optional)
};

[[nodiscard]] inline std::string iconRoleKeyName(IconRoleKey role)
{
    switch (role) {
    case IconRoleKey::WindowIcon: return "windowIcon";
    case IconRoleKey::WindowIconSymbolic: return "windowIconSymbolic";
    case IconRoleKey::Warning: return "warning";
    case IconRoleKey::RowCollapsed: return "rowCollapsed";
    case IconRoleKey::RowExpanded: return "rowExpanded";
    }
    return {};
}

} // namespace Ui::Res::Key
