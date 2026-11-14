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

#include <string>

namespace Ui::Action {

// "OpenTabSwitcher": the Ctrl+Tab list of the tabs most recent first - on the one viewed
// before the active tab for "next" (Ctrl+Tab), on the least recent for "prior"
// (Ctrl+Shift+Tab). Ctrl going up switches to the one highlighted; a quick tap does it
// with no list. Any other arg does nothing
template <typename Host>
void openTabSwitcher(Host & host, const std::string & arg)
{
    if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Next)) {
        host.openTabSwitcher(1);
    } else if (arg == Ui::Res::Key::tabStepKeyName(Ui::Res::Key::TabStepKey::Prior)) {
        host.openTabSwitcher(-1);
    }
}

} // namespace Ui::Action
