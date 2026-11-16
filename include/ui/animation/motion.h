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

#include "ui/animation/track.h"

namespace Ui::Animation {

// One element as the Animator keeps it: each property it animates on a track of its own
struct alignas(128) motion_t final {
    track_t left {};
    track_t top {};
    track_t width {};
    track_t height {};
    track_t background {};
    track_t color {};
    bool    isSeen = false; // drawn since the last sweep
};

} // namespace Ui::Animation
