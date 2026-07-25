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

// "Reload": reload all resources (res/*.json) live. reloadChrome owns the whole
// cycle - preserve any open popup/dialog, replay the load cycle (framework res,
// then the host's own res + feature gate via the hooks it passed to initialize),
// re-run the disable pass because loadAll resets enabled from JSON, apply every
// Changed bit, reopen the chrome. A host needs no override: its domain reload is
// already one of those hooks.
template <class Host>
void reload(Host & host, const std::string & /*arg*/)
{
    host.reloadChrome();
}

} // namespace Ui::Action
