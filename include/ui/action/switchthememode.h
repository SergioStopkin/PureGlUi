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

#include <iostream>
#include <string>

namespace Ui::Action {

// "SwitchThemeMode": toggle dark/light, re-apply the diff, refresh content.
template <typename Host>
void switchThemeMode(Host & host, const std::string & /*arg*/)
{
    const auto t0 = Common::Time::now();
    host.resManager().switchThemeMode();
    const auto t1 = Common::Time::now();
    host.windowManager().apply(host.resManager().changed());
    const auto t2 = Common::Time::now();
    host.windowManager().requestContentRefresh();
    std::cout << "[Action] Theme switched: load=" << Common::Time::msBetween(t0, t1)
              << "ms apply=" << Common::Time::msBetween(t1, t2) << "ms total=" << Common::Time::msSince(t0) << "ms"
              << std::endl;
}

} // namespace Ui::Action
