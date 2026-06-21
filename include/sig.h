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

#include <functional>
#include <string_view>

// Install the stop callback invoked when a termination signal is caught. The
// caller supplies how to stop (Shell::requestStop), so this stays free of any
// framework type.
void g_app_init(std::function<void()> stopCallback) noexcept;

// Signal helpers
[[nodiscard]] std::string_view signal_name(int sig) noexcept;
void                           on_signal(int sig) noexcept;
