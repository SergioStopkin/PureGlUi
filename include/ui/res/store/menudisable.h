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

#include "ui/res/type/menu.h"
#include "ui/type.h"

#include <algorithm>
#include <functional>
#include <ranges>
#include <vector>

namespace Ui::Res::Store {

// Disable menu nodes that can do nothing: a leaf whose actionKey has no
// registered handler, or a parent (submenu or top-menu-bar entry) whose every
// non-separator child ends up disabled. Recurses bottom-up so nested submenus -
// and the top-menu entries - collapse when they would only open dead ends.
// `isHandled(actionKey)` reports whether a handler exists; items that open a
// dialog stay enabled (the shell drives them without an Action::Registry entry).
// Only ever sets enabled=false (never re-enables), so a host-disabled node stays
// disabled and still counts toward its parent collapsing.
inline void disableUnhandled(std::vector<Ui::Res::Type::menu_t> & menus, const Ui::predicate_fn_t & isHandled)
{
    // Flatten the tree pre-order, then walk it reversed: every child is
    // processed before its parent, which replaces the bottom-up recursion.
    std::vector<std::reference_wrapper<Ui::Res::Type::menu_t>> order;
    std::vector<std::reference_wrapper<Ui::Res::Type::menu_t>> pending(menus.begin(), menus.end());
    while (!pending.empty()) {
        Ui::Res::Type::menu_t & node = pending.back();
        pending.pop_back();
        order.emplace_back(node);
        for (Ui::Res::Type::menu_t & child : node.items) {
            pending.emplace_back(child);
        }
    }

    for (Ui::Res::Type::menu_t & item : std::ranges::reverse_view(order)) {
        if (!item.items.empty()) {
            const bool anyEnabledChild = std::any_of(
            item.items.begin(),
            item.items.end(),
            [](const Ui::Res::Type::menu_t & child) { return !child.separator && child.enabled; });
            if (!anyEnabledChild) {
                item.enabled = false;
            }
            continue;
        }
        const bool isLeaf = item.submenu.empty() && item.dialog.title.empty() && !item.actionKey.empty();
        if (isLeaf && !isHandled(item.actionKey)) {
            item.enabled = false;
        }
    }
}

} // namespace Ui::Res::Store
