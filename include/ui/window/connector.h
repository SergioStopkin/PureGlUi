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
#include "ui/render/elementevent.h"
#include "ui/res/type/changed.h"
#include "ui/type.h"

#include <concepts>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace Ui::Window {

// The window family: a pure presentation surface (GL context + geometry).
template <typename T>
concept Window = requires(T window) {
    window.makeCurrent();
    window.swapBuffers();
    window.requestRender();
};

// The renderer family: draws frames; consumes pointer events via IEventApp.
template <typename T>
concept Renderer = std::derived_from<T, Ui::IRenderer>;

// Binds one window to one renderer. Windows and renderers are independent
// families that never reference each other; this is the only place a pairing
// exists, and the glue that needs both sides (make current -> draw,
// event-to-redraw coupling) lives here. Implements Ui::IRenderer itself -
// every forwarded call makes the window's context current first - so the
// coordinator can drive any pairing type-erased (render/resize/apply/
// readPixels + the IEventApp events).
//
// Lifetime is structural: the renderer member is declared after the window,
// so it is destroyed first - GL teardown always runs while the window's
// context is alive. The renderer is emplaced after window.create() because
// renderers need a live GL context to construct.
template <Window TWindow, Renderer TRenderer>
class Connector final : public Ui::IRenderer, private Common::NonCopyable {
    TWindow                  m_window;   // constructed first, destroyed last
    std::optional<TRenderer> m_renderer; // emplaced post-create; dies before the window

public:
    template <typename... WindowArgs>
    explicit Connector(WindowArgs &&... windowArgs)
        : m_window(std::forward<WindowArgs>(windowArgs)...)
    {
    }

    ~Connector() override { resetRenderer(); }

    [[nodiscard]] TWindow &       window() { return m_window; }
    [[nodiscard]] const TWindow & window() const { return m_window; }

    template <typename... RendererArgs>
    TRenderer & emplaceRenderer(RendererArgs &&... rendererArgs)
    {
        m_window.makeCurrent(); // renderers construct GL objects
        return m_renderer.emplace(std::forward<RendererArgs>(rendererArgs)...);
    }

    [[nodiscard]] bool        hasRenderer() const { return m_renderer.has_value(); }
    [[nodiscard]] TRenderer & renderer() { return *m_renderer; }

    // Explicit early teardown (renderer GL objects die with the context current).
    void resetRenderer()
    {
        if (m_renderer.has_value()) {
            m_window.makeCurrent();
            m_renderer.reset();
        }
    }

    // -------- Ui::IRenderer: make current, then forward --------
    // No swap in render(): callers present explicitly (swapBuffers or a
    // composite capture), matching every coordinator flow.

    bool render() override
    {
        if (!m_renderer.has_value()) {
            return false;
        }
        m_window.makeCurrent();
        return m_renderer->render();
    }

    void refresh() override
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->refresh();
    }

    void resize(Ui::fpx_t width, Ui::fpx_t height) override
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->resize(width, height);
    }

    void apply(Ui::Res::Type::Changed changed) override
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->apply(changed);
    }

    void cleanup() override
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->cleanup();
    }

    std::vector<uint8_t> readPixels(int & outWidth, int & outHeight) override
    {
        if (!m_renderer.has_value()) {
            return Ui::IRenderer::readPixels(outWidth, outHeight);
        }
        m_window.makeCurrent();
        return m_renderer->readPixels(outWidth, outHeight);
    }

    // -------- Ui::IEventApp: renderer consumes, window re-renders --------

    bool onMouseMove(int x, int y) override
    {
        if (m_renderer.has_value() && m_renderer->onMouseMove(x, y)) {
            m_window.requestRender();
            return true;
        }
        return false;
    }

    Ui::Render::element_event_t onMousePress(int                     x,
                                             int                     y,
                                             Ui::Window::MouseButton button,
                                             int                     clickCount,
                                             Ui::Window::KeyModifier modifiers) override
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->onMousePress(x, y, button, clickCount, modifiers);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }

    Ui::Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button) override
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->onMouseRelease(x, y, button);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }

    bool onMouseLeave() override
    {
        if (m_renderer.has_value() && m_renderer->onMouseLeave()) {
            m_window.requestRender();
            return true;
        }
        return false;
    }

    Ui::Render::element_event_t onScroll(int x, int y, Ui::fpx_t deltaY) override
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->onScroll(x, y, deltaY);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }
};

} // namespace Ui::Window
