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
#include "ui/interface/ieventapp.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/changed.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Ui {

/**
 * @brief Base interface for all renderers (UI chrome, content surfaces, etc.)
 *
 * Defines minimal common operations (render/resize/apply/events/readPixels).
 * Renderer-specific methods belong in derived classes.
 *
 * NOTE: distinct from Ui::IRender - IRender is the low-level draw sink
 * (fillRect/drawText/...); IRenderer is a renderer OBJECT's lifecycle contract.
 */
class IRenderer : public IEventApp {
public:
    ~IRenderer() override = default;

    /**
     * @brief Render the current frame
     * @return true if frame was rendered
     */
    virtual bool render() = 0;

    /**
     * @brief Handle window resize
     */
    virtual void resize(fpx_t width, fpx_t height) = 0;

    /**
     * @brief Apply resource changes (theme, layout, etc.)
     */
    virtual void apply(Ui::Res::Type::Changed changed) = 0;

    /**
     * @brief Re-present the last rendered frame without clearing or re-drawing
     *
     * Used after the parent window swaps buffers to ensure this window's
     * content remains visible (needed in fullscreen/unredirected compositing).
     * Default is no-op; a content-surface renderer may override it.
     */
    virtual void refresh() { }

    /**
     * @brief Cleanup resources
     */
    virtual void cleanup() = 0;

    /**
     * @brief Get status text (e.g. content stats for a content-surface renderer)
     */
    [[nodiscard]] virtual const std::string & statusText() const
    {
        static const std::string empty;
        return empty;
    }

    /**
     * @brief Read the rendered surface as RGBA pixels (top-left origin).
     *
     * Lets the window layer snapshot any content surface for compositing,
     * without knowing the renderer type. Default is empty (renderers that are
     * never composited - the UI chrome, popups - need not implement it); a
     * content-surface renderer overrides it with a framebuffer read.
     */
    virtual std::vector<uint8_t> readPixels(int & outWidth, int & outHeight)
    {
        outWidth  = 0;
        outHeight = 0;
        return {};
    }
};

} // namespace Ui
