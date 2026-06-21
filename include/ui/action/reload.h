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

// "Reload": reload all framework resources (res/*.json) live. reloadChrome
// preserves any open popup/dialog across the reload; loadAll re-marks every
// Changed bit so apply() rebuilds layout/theme/fonts; the disable pass re-runs
// because loadAll resets enabled from JSON. A host overrides "Reload" to also
// reload its domain resources.
template <class Host>
void reload(Host & host, const std::string & /*arg*/)
{
    host.reloadChrome([&host]() {
        host.resManager().loadAll();
        host.disableUnhandledMenuItems();
        host.windowManager().apply(host.resManager().changed());
        host.windowManager().requestContentRefresh();
    });
}

} // namespace Ui::Action
