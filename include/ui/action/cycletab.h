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

#include "ui/tabbar.h"
#include "ui/type.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

namespace Ui::Action {

// "CycleTab": the tab after the active one, the first after the last, activated
// as a click on it would be
template <typename Host>
void cycleTab(Host & host, const std::string & /*arg*/)
{
    const Ui::TabBar &        tabBar = host.resManager().tabBar();
    const std::vector<id_t> & order  = tabBar.order();
    const auto active = std::ranges::find_if(order, [&tabBar](id_t id) { return tabBar.find(id)->isActive; });
    if (active == order.end()) {
        return;
    }
    const auto after = std::next(active);
    const id_t next  = (after == order.end()) ? order.front() : *after;
    if (next != *active) {
        host.setActiveTab(next);
    }
}

} // namespace Ui::Action
