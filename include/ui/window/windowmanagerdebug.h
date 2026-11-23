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

namespace Ui::Window {

// Flip to true to trace what the window manager does: how a raw OS mouse event is
// routed to main/popup/content surface, which windows each tick renders, the
// popup and submenu lifecycle, and main-window resizes.
// Pair with WIN32_EVENT_DEBUG to see the full press->classify->dispatch chain.
constexpr bool WM_ROUTE_DEBUG = false;

} // namespace Ui::Window
