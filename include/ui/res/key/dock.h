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

// res/dock/*.json config keys - the single source of truth for their
// spelling, resolved through dockKeyName(). These are fw-invented keys, so
// they are camelCase like every other invented key (lastOpenDir, themeMode);
// kebab is reserved for real CSS property/selector spellings.
enum class DockKey : unsigned char {
    Name,         // dock identity (camelCase value; filename is cosmetic)
    Anchor,       // left/right
    Order,        // ordering among docks on the same anchor
    DefaultWidth, // initial width ("280px")
};

[[nodiscard]] inline std::string dockKeyName(DockKey key)
{
    switch (key) {
    case DockKey::Name: return "name";
    case DockKey::Anchor: return "anchor";
    case DockKey::Order: return "order";
    case DockKey::DefaultWidth: return "defaultWidth";
    }
    return {};
}

} // namespace Ui::Res::Key
