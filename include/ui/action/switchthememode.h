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

#include <chrono>
#include <iostream>
#include <string>

namespace Ui::Action {

// "SwitchThemeMode": toggle dark/light, re-apply the diff, refresh content.
template <typename Host>
void switchThemeMode(Host & host, const std::string & /*arg*/)
{
    auto t0 = std::chrono::steady_clock::now();
    host.resManager().switchThemeMode();
    auto t1 = std::chrono::steady_clock::now();
    host.windowManager().apply(host.resManager().changed());
    auto t2 = std::chrono::steady_clock::now();
    host.windowManager().requestContentRefresh();
    auto t3 = std::chrono::steady_clock::now();
    auto ms = [&](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
    std::cout << "[Action] Theme switched: load=" << ms(t0, t1) << "ms apply=" << ms(t1, t2)
              << "ms total=" << ms(t0, t3) << "ms" << std::endl;
}

} // namespace Ui::Action
