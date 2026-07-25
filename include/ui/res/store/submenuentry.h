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

namespace Ui::Res::Store {

/**
 * @brief One source file discovered while building an auto-submenu.
 *
 * MenuStore collects these from a submenu directory, sorts them (order, then key
 * as the tie-break so the result is stable regardless of filesystem listing
 * order), and turns each into a menu_t. Lives in Store, not Res::Type: it is the
 * walker's intermediate, never part of the res value vocabulary a host reads.
 */
struct alignas(128) submenu_entry_t final {
    int         order = 0; // JSON "order"; ties broken by key
    std::string key;       // lowercased display name - the item label, and the value passed to the action
    std::string display;   // JSON "name" as authored, registered in LocaleManager
};

} // namespace Ui::Res::Store
