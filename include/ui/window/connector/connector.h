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

#include "common/noncopyable.h"
#include "ui/interface/irenderer.h"
#include "ui/interface/iwindow.h"
#include "ui/window/connector/pairing.h"

#include <concepts>
#include <optional>
#include <utility>

namespace Ui::Window {

// The window family: a pure presentation surface (GL context + geometry).
template <typename T>
concept Window = std::derived_from<T, Ui::IWindow>;

// The renderer family: draws frames; consumes pointer events via IEventApp.
template <typename T>
concept Renderer = std::derived_from<T, Ui::IRenderer>;

// Owns one window and the renderer drawing into it, each by its own type. Windows and
// renderers are independent families that never reference each other; this is the only
// place a pairing exists. What drives the two - make current, then draw; an event that
// changed something, a frame - is its Pairing (pairing()), no template.
//
// Lifetime is structural: the renderer member is declared after the window,
// so it is destroyed first - GL teardown always runs while the window's
// context is alive. The renderer is emplaced after window.create() because
// renderers need a live GL context to construct.
template <Window TWindow, Renderer TRenderer>
class Connector final : private Common::NonCopyable {
public:
    template <typename... WindowArgs>
    explicit Connector(WindowArgs &&... windowArgs)
        : m_window(std::forward<WindowArgs>(windowArgs)...)
    {
    }

    ~Connector() { resetRenderer(); }

    [[nodiscard]] TWindow &       window() { return m_window; }
    [[nodiscard]] const TWindow & window() const { return m_window; }

    template <typename... RendererArgs>
    TRenderer & emplaceRenderer(RendererArgs &&... rendererArgs)
    {
        m_window.makeCurrent(); // renderers construct GL objects
        TRenderer & emplaced = m_renderer.emplace(std::forward<RendererArgs>(rendererArgs)...);
        m_pairing.attach(emplaced);
        return emplaced;
    }

    [[nodiscard]] bool        hasRenderer() const { return m_renderer.has_value(); }
    [[nodiscard]] TRenderer & renderer() { return *m_renderer; }

    // The window and renderer driven as one: frames, resize, apply and the pointer's events
    [[nodiscard]] Pairing & pairing() { return m_pairing; }

    // Explicit early teardown (renderer GL objects die with the context current).
    void resetRenderer()
    {
        if (m_renderer.has_value()) {
            m_pairing.detach();
            m_window.makeCurrent();
            m_renderer.reset();
        }
    }

private:
    TWindow                  m_window;   // constructed first, destroyed last
    std::optional<TRenderer> m_renderer; // emplaced post-create; dies before the window
    Pairing                  m_pairing { m_window };
};

} // namespace Ui::Window
