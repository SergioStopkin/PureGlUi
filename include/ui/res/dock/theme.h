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

#include "ui/color.h"
#include "ui/res/type/colorpair.h"

namespace Ui::Res::Dock {

// Visual style for the dock primitive. Shared across all docks (left/right,
// inner/outer) - per-instance theming would split this struct, costs nothing
// to keep unified until that need is real. Grip = the inkscape-style
// 3-vertical-dot resize handle on the viewport-facing edge of each dock.
struct alignas(128) dock_theme_t final {
    Ui::Res::Type::color_pair_t background; // content area
    Ui::Res::Type::color_pair_t grip;       // resize handle strip (idle)
    Ui::Res::Type::color_pair_t gripHover;  // hover over the strip
    Ui::Res::Type::color_pair_t gripActive; // mid-drag
    // Slider rows. Its own pairs rather than reusing the grip's: the grip is a
    // strip at the dock edge with a separator beside it, so it reads at
    // near-background colours that a bar inside the dock body would not.
    //
    // Two pairs for three drawn pieces, because the filled portion and the thumb
    // are one idea - where the value is - and only the track is the other. The
    // thumb pair carries both.
    Ui::Res::Type::color_pair_t sliderTrack;      // the bar behind the value
    Ui::Res::Type::color_pair_t sliderThumb;      // grip, and the track filled up to it
    Ui::Res::Type::color_pair_t sliderThumbHover; // grip under the pointer
    Ui::Color                   separator;        // 1px line between content and viewport

    bool operator==(const dock_theme_t &) const = default;
};

} // namespace Ui::Res::Dock
