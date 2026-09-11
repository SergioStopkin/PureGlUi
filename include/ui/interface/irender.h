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
#include "ui/render/shadow.h"
#include "ui/res/type/alignh.h"
#include "ui/res/type/alignv.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/font.h"
#include "ui/type.h"

#include <string_view>

namespace Ui {

// Domain-blind draw sink for the UI framework. The framework traverses its
// layout and issues immediate primitive calls; a backend (GL today) implements
// them. The interface speaks only in already-relocated fw value types - it
// never sees menus, actions, or workspaces.
//
// Frame protocol: beginFrame() once, any number of draw calls, endFrame().
// A backend may buffer calls (e.g. batch text by font) and flush in endFrame().
// Draw order is the call order, except text, which a backend is free to defer
// so it composites above non-text in the same frame.
class IRender {
public:
    virtual ~IRender() = default;

    // Frame lifecycle. beginFrame sets the viewport (physical px) and resets
    // GL state; endFrame flushes any buffered work.
    virtual void beginFrame(fpx_t width, fpx_t height) = 0;
    virtual void endFrame()                            = 0;

    // Rounded rectangle fill with per-corner radii. colors.fg is the fill;
    // colors.bg is the antialiasing/compositing background. An invisible
    // shadow ({}), is skipped.
    virtual void fillRect(const Res::Type::bound_t &      bound,
                          const Res::Type::border_t &     radii,
                          const Res::Type::color_pair_t & colors,
                          const Render::shadow_t &        shadow) = 0;

    // Text run inside pos, placed on both axes. minPadH is the minimum side
    // padding, used by Left/Right and by CenterClamped as its fallback edge.
    // The backend owns baseline placement from the font handle.
    virtual void drawText(font_handle_t              font,
                          std::string_view           text,
                          const Res::Type::bound_t & pos,
                          const Color &              color,
                          Res::Type::AlignH          alignH,
                          Res::Type::AlignV          alignV,
                          fpx_t                      minPadH) = 0;

    // Image (SVG or raster, inferred from src) scaled into bound with the given
    // corner radii. tint with a() > 0 recolors; scale > 1 enlarges around the
    // center (icon hover/active). An invisible shadow ({}) is skipped.
    // isFilled: an outline-authored SVG (fill="none") is drawn as a solid shape.
    // False keeps it as authored - the look popup chrome already gets, since it
    // loads the plain document.
    virtual void drawImage(std::string_view            src,
                           const Res::Type::bound_t &  bound,
                           const Res::Type::border_t & radii,
                           const Color &               tint,
                           fpx_t                       scale,
                           const Render::shadow_t &    shadow,
                           bool                        isFilled) = 0;

    // Pre-warm an image into the sink's cache at the given scale so a later
    // drawImage at active scale is hitch-free (icon hover/active). Same args as
    // the eventual drawImage minus the radii/shadow the warm pass does not need.
    virtual void warmImage(std::string_view           src,
                           const Res::Type::bound_t & bound,
                           const Color &              tint,
                           fpx_t                      scale,
                           bool                       isFilled) = 0;

    // Flat-color triangle, vertices in CSS px. Used for the tab loading-bar
    // arrow tip - the one shape that is not a rounded rect.
    virtual void drawTriangle(fpx_t x0, fpx_t y0, fpx_t x1, fpx_t y1, fpx_t x2, fpx_t y2, const Color & color) = 0;

    // Measurement / resources used while producing draw calls. Text is wide
    // (std::wstring, the project's canonical text type) so measurement and
    // truncation stay codepoint-safe for UTF-16/UTF-32 symbols.
    virtual fpx_t         textWidth(font_handle_t font, std::wstring_view text) = 0;
    virtual font_handle_t createFont(const Res::Type::font_t & font)            = 0;
};

} // namespace Ui
