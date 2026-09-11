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

#include "ui/render/elementevent.h"
#include "ui/type.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

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
    // Every button is delivered, not just Left - a content surface needs middle
    // for pan and right for a context menu. Chrome implementations therefore
    // have to ignore what they do not handle rather than assume Left.
    //
    // clickCount: 1=single press, 2=double, ... Populated by the platform event
    // layer. No default arg (prohibited on virtuals); callers that don't care
    // about multi-click pass 1 explicitly.
    // modifiers: held at press time, so a handler can tell a plain click from
    // an additive Ctrl+click without tracking key state of its own.
    // Press and scroll report an element_event_t rather than a bool so a drag
    // or a wheel routes the same way a click does. `changed` carries what the
    // bool used to mean: this needs a repaint.
    virtual Render::element_event_t
    onMousePress(int x, int y, Ui::Window::MouseButton button, int clickCount, Ui::Window::KeyModifier modifiers) = 0;
    virtual Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button)                  = 0;
    virtual bool                    onMouseLeave()                                                                = 0;
    virtual Render::element_event_t onScroll(int x, int y, fpx_t deltaY)                                          = 0;
};

} // namespace Ui
