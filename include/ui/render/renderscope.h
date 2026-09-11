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

#include <cstdint>

namespace Ui::Render {

// How much must be redrawn after a binding fires, so a call site declares the
// class of repaint instead of remembering which refresh call to make.
//
// Deliberately coarse, and named for what the framework can actually do: the
// smallest addressable unit is one OS surface. A dock is NOT a surface - it
// appends ops into the main window's frame - so there is no per-dock scope and
// pretending otherwise would be a lie in the type system.
//
// No host content surface value either: only the host knows its content
// changed, so it requests that repaint itself.
enum class RenderScope : uint8_t {
    None,   // nothing to repaint
    Chrome, // re-emit ops against the existing layout - requestMainRender()
    Layout, // rebuild the layout first, then repaint  - requestContentRefresh()
};

} // namespace Ui::Render
