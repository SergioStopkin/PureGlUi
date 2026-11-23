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

#include "ui/window/connector/mainconnector.h"
#include "ui/window/eventfactory.h"
#include "ui/window/pointershape.h"

#include <memory>

namespace Ui::Window {

// What the pointer shows, applied to whichever seam the platform owns it on: the
// window (X11 / Win32 / macOS) or the pointer (Wayland, which must re-set
// it on every enter). Both are no-ops where they do not apply, so this
// sets both rather than branching on platform.
class PointerCursor final {
public:
    // `main` and `events` are the window manager's, empty until it makes them
    PointerCursor(const std::unique_ptr<MainConnector> & main, const std::unique_ptr<NativeEvent> & events)
        : m_main(main)
        , m_events(events)
    {
    }

    void set(Ui::Window::PointerShape shape)
    {
        if (m_shape == shape) {
            return;
        }
        m_shape = shape;
        if (m_main) {
            // Through the window: m_main is the window+renderer pairing, and the
            // cursor belongs to the surface, not to whatever draws into it
            m_main->window().setCursor(shape);
        }
        if (m_events) {
            m_events->setCursor(shape);
        }
    }

    [[nodiscard]] Ui::Window::PointerShape shape() const { return m_shape; }

private:
    const std::unique_ptr<MainConnector> & m_main;
    const std::unique_ptr<NativeEvent> &   m_events;
    Ui::Window::PointerShape               m_shape = Ui::Window::PointerShape::Default;
};

} // namespace Ui::Window
