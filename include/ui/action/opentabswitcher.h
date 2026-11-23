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

#include "common/time.h"
#include "ui/res/key/tabstep.h"

#include <chrono>
#include <string>
#include <string_view>

namespace Ui::Action {

// Its "action" key, as shortcut.json binds it; the shell steps an open list on that binding
inline constexpr std::string_view OPEN_TAB_SWITCHER = "OpenTabSwitcher";

// "OpenTabSwitcher": the Ctrl+Tab list of the tabs most recent first - on the one viewed
// before the active tab for "next" (Ctrl+Tab), on the least recent for "prior"
// (Ctrl+Shift+Tab); open already, the key that opened it steps it. It goes up once Ctrl has
// been held past input.json's tabSwitcher.delayMs, so a quick tap switches with none; Ctrl
// going up switches to the one highlighted. Not over a dialog; an open menu closes. Any
// other arg does nothing
template <typename Host>
void openTabSwitcher(Host & host, const std::string & arg)
{
    int delta = 0;
    if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Next)) {
        delta = 1;
    } else if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Prior)) {
        delta = -1;
    } else {
        return;
    }
    if (host.tabSwitch().isOpen()) {
        host.cycleTabSwitcher(delta);
        return;
    }
    if (host.windowManager().dialog().isOpen()) {
        return;
    }
    if (host.isPopupOpen()) {
        host.closePopupMenu();
    }
    host.tabSwitch().open(
        host.resManager().tabBar().recentIds(),
        delta,
        Common::Time::now() + std::chrono::milliseconds(host.resManager().input().tabSwitcherDelayMs));
}

} // namespace Ui::Action
