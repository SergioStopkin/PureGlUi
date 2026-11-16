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

#include "ui/render/bgop.h"
#include "ui/render/imageop.h"
#include "ui/render/progressop.h"
#include "ui/render/textop.h"

#include <vector>

namespace Ui::Render {

// One layer of a frame's batched draws, drawn backgrounds, progress, images, then text -
// a layer drawn after it covers it whole
struct alignas(128) draw_layer_t final {
    std::vector<bg_op_t>       bgOps;
    std::vector<progress_op_t> progressOps;
    std::vector<image_op_t>    imageOps;
    std::vector<text_op_t>     textOps;

    void clear()
    {
        bgOps.clear();
        progressOps.clear();
        imageOps.clear();
        textOps.clear();
    }

    [[nodiscard]] bool empty() const
    {
        return bgOps.empty() && progressOps.empty() && imageOps.empty() && textOps.empty();
    }
};

} // namespace Ui::Render
