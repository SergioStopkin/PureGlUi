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

#include "ui/render/clickresult.h"
#include "ui/type.h"

namespace Ui {

/**
 * @brief Abstract interface for application-level event handling
 *
 * Shared by windows, renderers, and WindowManager.
 * Provides mouse event methods that flow through the chain:
 * WindowManager -> IWindow -> IRenderer.
 * Self-rooted (own virtual dtor) - the fw avoids a shared interface base.
 */
class IEventApp {
public:
    virtual ~IEventApp() = default;

    virtual bool onMouseMove(int x, int y) = 0;
    // clickCount: 1=single press, 2=double, ... Populated by the platform
    // event layer; defaulted at the base so callers that don't care can
    // still write onMousePress(x,y) and get single-click semantics.
    virtual bool                   onMousePress(int x, int y, int clickCount = 1) = 0;
    virtual Render::click_result_t onMouseRelease(int x, int y)                   = 0;
    virtual bool                   onMouseLeave()                                 = 0;
    virtual bool                   onScroll(int x, int y, fpx_t deltaY)           = 0;
};

} // namespace Ui
