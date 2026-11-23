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
#include "ui/render/elementevent.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/changed.h"
#include "ui/type.h"
#include "ui/window/keymodifier.h"
#include "ui/window/mousebutton.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Ui::Window {

// A window and the renderer drawing into it, driven as one: every call makes the window's
// context current first, and an event that changed what the renderer shows asks the window
// for a frame. The one place that glue lives, and what a coordinator drives a pairing by,
// blind - its own windows and a host's content surface alike. A Connector holds one over
// the window and renderer it owns
class Pairing final : private Common::NonCopyable {
public:
    explicit Pairing(Ui::IWindow & window)
        : m_window(window)
    {
    }

    // The renderer drawing into the window, from its emplace until its reset
    void attach(Ui::IRenderer & renderer) { m_renderer = renderer; }
    void detach() { m_renderer.reset(); }

    [[nodiscard]] Ui::IWindow & window() const { return m_window; }

    // -------- Frames: make current, then forward --------
    // No swap in render(): callers present explicitly (swapBuffers or a
    // composite capture), matching every coordinator flow.

    bool render()
    {
        if (!m_renderer.has_value()) {
            return false;
        }
        m_window.makeCurrent();
        return m_renderer->get().render();
    }

    void resize(Ui::fpx_t width, Ui::fpx_t height)
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->get().resize(width, height);
    }

    void apply(Ui::Res::Type::changes_t changed)
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->get().apply(changed);
    }

    void cleanup()
    {
        if (!m_renderer.has_value()) {
            return;
        }
        m_window.makeCurrent();
        m_renderer->get().cleanup();
    }

    // What the renderer shows in `region`, none with no renderer
    std::vector<uint8_t> readPixels(const Ui::Res::Type::bound_t & region)
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        m_window.makeCurrent();
        return m_renderer->get().readPixels(region);
    }

    // -------- Events: the renderer consumes, the window re-renders --------

    bool onMouseMove(int x, int y)
    {
        if (m_renderer.has_value() && m_renderer->get().onMouseMove(x, y)) {
            m_window.requestRender();
            return true;
        }
        return false;
    }

    Ui::Render::element_event_t
    onMousePress(int x, int y, Ui::Window::MouseButton button, int clickCount, Ui::Window::key_modifiers_t modifiers)
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->get().onMousePress(x, y, button, clickCount, modifiers);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }

    Ui::Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button)
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->get().onMouseRelease(x, y, button);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }

    bool onMouseLeave()
    {
        if (m_renderer.has_value() && m_renderer->get().onMouseLeave()) {
            m_window.requestRender();
            return true;
        }
        return false;
    }

    Ui::Render::element_event_t onScroll(int x, int y, Ui::fpx_t deltaY)
    {
        if (!m_renderer.has_value()) {
            return {};
        }
        Ui::Render::element_event_t result = m_renderer->get().onScroll(x, y, deltaY);
        if (result.changed) {
            m_window.requestRender();
        }
        return result;
    }

private:
    Ui::IWindow &                                        m_window;
    std::optional<std::reference_wrapper<Ui::IRenderer>> m_renderer; // none before the emplace and after the reset
};

} // namespace Ui::Window
