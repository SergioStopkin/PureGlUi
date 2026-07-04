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

#include "ui/interface/irenderer.h"
#include "ui/res/type/border.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Ui {

/**
 * @brief Interface for popup-type renderers with software rounded corners
 *
 * Extends IRenderer with corner pixel upload for SDF-based corner blending.
 * Implemented by PopupRenderer and DialogRenderer.
 */
class IPopupRenderer : public Ui::IRenderer {
public:
    ~IPopupRenderer() override = default;

    /**
     * @brief Upload corner pixel textures for software rounded corners
     */
    virtual void setCornerPixels(std::array<std::vector<uint8_t>, 4> pixels, const Ui::Res::Type::border_t & radii) = 0;

    /**
     * @brief Check if corner pixel textures are loaded
     */
    [[nodiscard]] virtual bool hasCornerPixels() const = 0;

    /**
     * @brief Set alpha blending mode
     */
    virtual void setAlpha(bool hasAlpha) = 0;
};

} // namespace Ui
