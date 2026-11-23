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

#include "ui/window/compositetexture.h"

#include <memory>

namespace Ui::Window {

// One popup-type window the WindowManager owns: its connector (window + renderer), none
// while it is closed, and the frame it was captured into, composited, to draw it into the
// main window
template <typename TConnector>
struct alignas(64) popup_slot_t final {
    std::unique_ptr<TConnector> connector;
    CompositeTexture            composite;
    bool isRecaptured = false; // composited, read and drawn in by this main pass (MainFrame::renderQueued)
};

} // namespace Ui::Window
