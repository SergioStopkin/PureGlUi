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

namespace Ui::Action {

// "SwitchTheme": select a theme by name (arg), re-apply the diff, refresh.
// No-op when the arg is empty or already the active theme.
template <typename Host>
void switchTheme(Host & host, const std::string & arg)
{
    if (!arg.empty() && arg != host.resManager().themeName()) {
        host.resManager().setThemeName(arg);
        host.windowManager().apply(host.resManager().changed());
        host.windowManager().requestContentRefresh();
    }
}

} // namespace Ui::Action
