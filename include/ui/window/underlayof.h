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
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Ui::Window {

// What stands under `area` (main-window physical px) for a see-through popup, RGBA, top-down and
// opaque: `background` where the area leaves the window; inside it the window's own pixels,
// `readWindow(region)` handing them bottom-up as GL reads them; and over those a content
// surface's, `readContent(region)` handing them top-down in its own px, where `contentBound`
// covers the area. A read of the wrong size is left out
[[nodiscard]] inline std::vector<std::uint8_t>
underlayOf(const Ui::Res::Type::bound_t &                                                   area,
           int                                                                              windowWidth,
           int                                                                              windowHeight,
           const Ui::Color &                                                                background,
           const std::function<std::vector<std::uint8_t>(const Ui::Res::Type::bound_t &)> & readWindow,
           const std::optional<Ui::Res::Type::bound_t> &                                    contentBound,
           const std::function<std::vector<std::uint8_t>(const Ui::Res::Type::bound_t &)> & readContent)
{
    const int areaX = static_cast<int>(area.x);
    const int areaY = static_cast<int>(area.y);
    const int areaW = static_cast<int>(area.w);
    const int areaH = static_cast<int>(area.h);
    if (areaW <= 0 || areaH <= 0) {
        return {};
    }

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(areaW) * areaH * 4);
    for (std::size_t at = 0; at < pixels.size(); at += 4) {
        pixels.at(at + 0) = background.r();
        pixels.at(at + 1) = background.g();
        pixels.at(at + 2) = background.b();
        pixels.at(at + 3) = 255;
    }
    // `source`'s rows, `sourceW` wide, bottom-up when `isBottomUp`, into the area's at main-window
    // pixel (left, top); one of the wrong size left out
    const auto blit = [&pixels, areaX, areaY, areaW](const std::vector<std::uint8_t> & source,
                                                     int                               left,
                                                     int                               top,
                                                     int                               sourceW,
                                                     int                               sourceH,
                                                     bool                              isBottomUp) {
        if (source.size() != static_cast<std::size_t>(sourceW) * sourceH * 4) {
            return;
        }
        for (int row = 0; row < sourceH; ++row) {
            const int         sourceRow = isBottomUp ? sourceH - 1 - row : row;
            const std::size_t from      = static_cast<std::size_t>(sourceRow) * sourceW * 4;
            const std::size_t to        = ((static_cast<std::size_t>(top - areaY + row) * areaW) + (left - areaX)) * 4;
            for (int column = 0; column < sourceW; ++column) {
                const std::size_t pixel   = static_cast<std::size_t>(column) * 4;
                pixels.at(to + pixel + 0) = source.at(from + pixel + 0);
                pixels.at(to + pixel + 1) = source.at(from + pixel + 1);
                pixels.at(to + pixel + 2) = source.at(from + pixel + 2);
            }
        }
    };

    // The part of the area inside the window
    const int left   = std::max(0, areaX);
    const int top    = std::max(0, areaY);
    const int right  = std::min(windowWidth, areaX + areaW);
    const int bottom = std::min(windowHeight, areaY + areaH);
    if (right <= left || bottom <= top) {
        return pixels;
    }
    blit(readWindow({ .x = static_cast<fpx_t>(left),
                      .y = static_cast<fpx_t>(top),
                      .w = static_cast<fpx_t>(right - left),
                      .h = static_cast<fpx_t>(bottom - top) }),
         left,
         top,
         right - left,
         bottom - top,
         true);

    if (!contentBound.has_value()) {
        return pixels;
    }
    const Ui::Res::Type::bound_t content       = *contentBound;
    const int                    overlapLeft   = std::max(left, static_cast<int>(content.x));
    const int                    overlapTop    = std::max(top, static_cast<int>(content.y));
    const int                    overlapRight  = std::min(right, static_cast<int>(content.x + content.w));
    const int                    overlapBottom = std::min(bottom, static_cast<int>(content.y + content.h));
    if (overlapRight > overlapLeft && overlapBottom > overlapTop) {
        blit(readContent({ .x = static_cast<fpx_t>(overlapLeft) - content.x,
                           .y = static_cast<fpx_t>(overlapTop) - content.y,
                           .w = static_cast<fpx_t>(overlapRight - overlapLeft),
                           .h = static_cast<fpx_t>(overlapBottom - overlapTop) }),
             overlapLeft,
             overlapTop,
             overlapRight - overlapLeft,
             overlapBottom - overlapTop,
             false);
    }
    return pixels;
}

} // namespace Ui::Window
