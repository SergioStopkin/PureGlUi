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

#include "ui/res/key/tabstep.h"
#include "ui/tabbar.h"
#include "ui/type.h"

#include <string>

namespace Ui::Action {

// "CycleTab": for "next" the tab right of the active one, the first after the last;
// for "prior" the one left of it, the last before the first - activated as a click
// on it would be. Any other arg does nothing
template <typename Host>
void cycleTab(Host & host, const std::string & arg)
{
    const Ui::TabBar & tabBar = host.resManager().tabBar();
    const id_t         active = tabBar.activeId();
    id_t               next   = active;
    if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Next)) {
        next = tabBar.idAfter(active);
    } else if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Prior)) {
        next = tabBar.idBefore(active);
    }
    if (next != active) {
        host.setActiveTab(next);
    }
}

} // namespace Ui::Action
