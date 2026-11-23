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
#include "ui/color.h"
#include "ui/config.h"
#include "ui/idkind.h"
#include "ui/interface/iwindow.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/changed.h"
#include "ui/type.h"
#include "ui/window/compositor.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/contenthit.h"
#include "ui/window/contentsurface.h"
#include "ui/window/geometry.h"
#include "ui/window/nativewindowhandle.h"
#include "ui/window/pointercursor.h"
#include "ui/window/pointershape.h"
#include "ui/window/renderqueue.h"
#include "ui/window/tooltip.h"

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace Ui::Window {

// The host's content surfaces - windows the host owns, registered here by its id, which
// doubles as the render-queue entry: only the active one is shown and takes events (the
// windows overlap), and the one the pointer is over hovers. Each stands at the viewport
// Geometry leaves
class HostSurfaces final : private Common::NonCopyable {
public:
    // All the window manager's, `main` empty until it makes the window
    HostSurfaces(const std::unique_ptr<MainConnector> & main,
                 const Geometry &                       geometry,
                 PointerCursor &                        cursor,
                 Ui::Window::RenderQueue &              renderQueue)
        : m_main(main)
        , m_geometry(geometry)
        , m_cursor(cursor)
        , m_renderQueue(renderQueue)
    {
    }

    // Register a host-owned content surface by its pairing, which must outlive the
    // registration. Its render request goes to the queue, and it stands in the viewport.
    // An id is added once: remove it before adding it again, or the first pairing stays registered
    void add(id_t id, Pairing & pairing)
    {
        if (!Ui::isHostId(id)) {
            std::cerr << "[WindowManager] Content surface id " << id << " carries no host kind" << std::endl;
        }
        if (!m_surfaces.try_emplace(id, pairing).second) {
            std::cerr << "[WindowManager] Content surface id " << id << " is already registered" << std::endl;
            return;
        }
        Ui::IWindow & content = pairing.window();
        content.setRenderRequest([this, id]() { m_renderQueue.request(id); });
        position(content);
    }

    void remove(id_t id)
    {
        auto it = m_surfaces.find(id);
        if (it == m_surfaces.end()) {
            return;
        }
        // Composite texture lives in the main GL context; delete it there.
        if (it->second.composite.texture != 0 && m_main) {
            m_main->window().makeCurrent();
            it->second.composite.destroy();
        }
        m_surfaces.erase(it);
        if (m_activeId == id) {
            m_activeId = Ui::INVALID_ID;
        }
        if (m_pointerId == id) {
            m_pointerId = Ui::INVALID_ID;
        }
    }

    // Make one content surface active: only it is shown (the windows overlap).
    void setActive(id_t id)
    {
        m_activeId = id;
        // The surfaces overlap, so a pointer over the one hidden now is over this
        // one: the hidden one has lost it, and this one's cursor shows
        if (m_pointerId != Ui::INVALID_ID && m_pointerId != id) {
            leave();
            if (const auto shown = m_surfaces.find(id); shown != m_surfaces.end()) {
                m_pointerId = id;
                m_cursor.set(shown->second.pointer);
            }
        }
        for (auto & [surfaceId, entry] : m_surfaces) {
            Ui::IWindow & content = entry.pairing.get().window();
            if (g_config.isCompositing) {
                // Composite mode: windows stay offscreen; just repaint the active one.
                if (surfaceId == id) {
                    content.requestRender();
                }
            } else if (surfaceId == id) {
                content.show();
                // show() alone doesn't repaint the viewport; queue a frame.
                content.requestRender();
            } else {
                content.hide();
            }
        }
    }

    // Mark a content surface ready (or not) for rendering. The host clears this
    // while a model loads asynchronously so the render loop skips the surface
    // (avoids empty-scene frames), and sets it once the load lands.
    void setReady(id_t id, bool isReady)
    {
        auto it = m_surfaces.find(id);
        if (it != m_surfaces.end()) {
            it->second.isReady = isReady;
        }
    }

    // Whether a content surface is ready to render (false while a host is mid
    // async-load). Unknown id -> false.
    [[nodiscard]] bool isReady(id_t id) const
    {
        auto it = m_surfaces.find(id);
        return it != m_surfaces.end() && it->second.isReady;
    }

    // What the pointer shows over a content surface - its host's to say, as a tool
    // armed there changes what a click does. At once when the pointer is over it -
    // a key arms a tool without a move - and from then on every move over it sets
    // it, on either route a move takes (main-window hit test or its own events)
    void setPointer(id_t id, Ui::Window::PointerShape shape)
    {
        auto it = m_surfaces.find(id);
        if (it == m_surfaces.end()) {
            return;
        }
        it->second.pointer = shape;
        if (id == m_pointerId) {
            m_cursor.set(shape);
        }
    }

    // A tooltip for something a content surface's host marks: `text` beside `anchor`, a
    // rect in the surface's physical pixels; an empty text for none
    void setTooltip(id_t id, const std::string & text, const Ui::Res::Type::bound_t & anchor)
    {
        auto it = m_surfaces.find(id);
        if (it != m_surfaces.end()) {
            it->second.tooltipText   = text;
            it->second.tooltipAnchor = anchor;
        }
    }

    // The tooltip of the surface under the pointer, its anchor moved into the viewport and
    // into the main window's CSS px where the toolbar's anchors are; none for none
    [[nodiscard]] tooltip_t tooltip() const
    {
        const auto surface = m_surfaces.find(m_pointerId);
        if (surface == m_surfaces.end() || surface->second.tooltipText.empty()) {
            return {};
        }
        const Ui::Res::Type::bound_t & anchor   = surface->second.tooltipAnchor;
        const Ui::Res::Type::bound_t   viewport = m_geometry.viewport();
        return { .text   = surface->second.tooltipText,
                 .anchor = { .x = toCss(viewport.x + anchor.x),
                             .y = toCss(viewport.y + anchor.y),
                             .w = toCss(anchor.w),
                             .h = toCss(anchor.h) } };
    }

    // The content-surface id of a native window handle, or INVALID_ID for the main
    // window / an unregistered handle
    [[nodiscard]] id_t idOf(Ui::Window::NativeWindowHandle handle) const
    {
        for (const auto & [surfaceId, surface] : m_surfaces) {
            if (surface.pairing.get().window().nativeHandle() == handle) {
                return surfaceId;
            }
        }
        return Ui::INVALID_ID;
    }

    // A content surface's window, none for an unknown id
    [[nodiscard]] std::optional<std::reference_wrapper<Ui::IWindow>> window(id_t id) const
    {
        const auto found = pairing(id);
        if (!found.has_value()) {
            return std::nullopt;
        }
        return found->get().window();
    }

    [[nodiscard]] std::optional<std::reference_wrapper<Ui::IWindow>> activeWindow() const { return window(m_activeId); }

    // The host's window+renderer pairing for a content surface - the sink for its events and
    // frame production; none for an unknown id
    [[nodiscard]] std::optional<std::reference_wrapper<Pairing>> pairing(id_t id) const
    {
        const auto it = m_surfaces.find(id);
        if (it == m_surfaces.end()) {
            return std::nullopt;
        }
        return it->second.pairing;
    }

    [[nodiscard]] std::optional<std::reference_wrapper<Pairing>> activePairing() const { return pairing(m_activeId); }

    // The active surface's bound while it is ready to draw, what is read under a popup;
    // none otherwise
    [[nodiscard]] std::optional<Ui::Res::Type::bound_t> activeReadyBound() const
    {
        const auto it = m_surfaces.find(m_activeId);
        if (it == m_surfaces.end() || !it->second.isReady) {
            return std::nullopt;
        }
        return it->second.pairing.get().window().bound();
    }

    // Hit-test the active content surface, returning child-local coords on hit
    // (see content_hit_t); none off it. Single lookup so the window and its bound match.
    [[nodiscard]] std::optional<content_hit_t> hitTestActive(int x, int y) const
    {
        const auto it = m_surfaces.find(m_activeId);
        if (it == m_surfaces.end()) {
            return std::nullopt;
        }
        const Ui::Res::Type::bound_t bound = it->second.pairing.get().window().bound();
        if (!bound.contains(x, y)) {
            return std::nullopt;
        }
        return content_hit_t { .pairing = it->second.pairing,
                               .lx      = x - static_cast<int>(bound.x),
                               .ly      = y - static_cast<int>(bound.y) };
    }

    // The pointer moved to `x`, `y` on the main window: the active surface's hit, which
    // it is now over or not
    std::optional<content_hit_t> hover(int x, int y)
    {
        const std::optional<content_hit_t> hit = hitTestActive(x, y);
        m_pointerId                            = hit.has_value() ? m_activeId : Ui::INVALID_ID;
        return hit;
    }

    // What the pointer shows over the active surface
    [[nodiscard]] Ui::Window::PointerShape activePointer() const
    {
        const auto it = m_surfaces.find(m_activeId);
        return it != m_surfaces.end() ? it->second.pointer : Ui::Window::PointerShape::Default;
    }

    // The pointer moved over surface `id` in its own window: its pointer shows, and its
    // pairing takes the move; none for no such surface
    std::optional<std::reference_wrapper<Pairing>> enter(id_t id)
    {
        const auto it = m_surfaces.find(id);
        if (it == m_surfaces.end()) {
            return std::nullopt;
        }
        m_pointerId = id;
        m_cursor.set(it->second.pointer);
        return it->second.pairing;
    }

    // The pointer has left the content surface it was over, if any: its hover,
    // snap, tooltip and pointer shape go with it
    void leave()
    {
        const auto left = pairing(m_pointerId);
        if (!left.has_value()) {
            return;
        }
        left->get().onMouseLeave();
        m_pointerId = Ui::INVALID_ID;
        m_cursor.set(Ui::Window::PointerShape::Default);
    }

    // The pointer left the app: over no surface
    void clearPointer() { m_pointerId = Ui::INVALID_ID; }

    // A popup-type window at `closed` (main-window physical px) closed or hid. Not
    // composited it was a window of its own, and the surface under it has to draw what
    // it covered - nothing else asks it to. Composited, the next main frame draws the
    // surface; one over the chrome only needs the main frame
    //
    // Call it before the window's reset(), passing its bound: it only queues a frame
    void redrawUnder(const Ui::Res::Type::bound_t & closed) const
    {
        if (g_config.isCompositing || !closed.overlaps(m_geometry.viewport())) {
            return;
        }
        if (const auto content = activeWindow()) {
            content->get().requestRender();
        }
    }

    // Every surface moved and sized to the viewport, each queued to draw again: one that
    // just grew holds the previous frame at the old size, so the strip it gained shows
    // stale pixels (composited: a stale texture) until something redraws it
    void fitToViewport()
    {
        for (auto & [id, entry] : m_surfaces) {
            fit(entry);
            entry.pairing.get().window().requestRender();
        }
    }

    // The same after the main window resized, the active one drawn and captured at once
    void fitToResizedWindow()
    {
        for (auto & [id, entry] : m_surfaces) {
            fit(entry);
            if (id == m_activeId) {
                Pairing & surface = entry.pairing;
                surface.window().requestRender();
                surface.render();
                if (g_config.isCompositing) {
                    capture(entry);
                }
            }
        }
    }

    // A res change handed on: the theme's workspace background to every window, the rest
    // to its pairing, which draws again. Each setBackground() makes its own context current
    void apply(Ui::Res::Type::changes_t changed, const Ui::Color & background)
    {
        for (auto & [id, entry] : m_surfaces) {
            Pairing & surface = entry.pairing;
            if (changed.has(Ui::Res::Type::Changed::Theme)) {
                surface.window().setBackground(background);
            }
            surface.apply(changed);
            surface.window().requestRender();
        }
    }

    /**
     * @brief Render the active content surface if queued.
     *
     * Returns true when the main window must re-render: composited, it draws the
     * surface; otherwise its frame is what recaptures the corners of a popup,
     * dialog or tooltip standing on the surface, `isChromeOver` saying one is.
     */
    bool render(bool isChromeOver)
    {
        bool isMainFrameNeeded = false;
        for (auto & [id, entry] : m_surfaces) {
            // Skip while the host is mid async-load (avoids empty-scene renders
            // from spurious resize events), and render only the active surface.
            if (!entry.isReady || id != m_activeId) {
                continue;
            }

            // Only render if in the render queue.
            if (!m_renderQueue.pending().contains(id)) {
                continue;
            }

            Pairing &  surface  = entry.pairing;
            const bool rendered = surface.render();

#ifdef HAVE_WAYLAND
            // Native Wayland: the surface uses our subsurface's EGL context but
            // can't swap (no EGL surface of its own). Restore + swap manually.
            if (rendered) {
                Ui::IWindow & content = surface.window();
                content.makeCurrent();
                content.swapBuffers();
                // A resize made while the surface held its next buffer comes with this swap, so the
                // frame it showed is the old size: drawn again at the new one
                if (!content.isShownAtItsSize() && surface.render()) {
                    content.makeCurrent();
                    content.swapBuffers();
                }
            }
#endif

            // Composite mode: capture rendered pixels for main-window compositing.
            if (rendered && g_config.isCompositing) {
                capture(entry);
            }
            if (rendered && (g_config.isCompositing || isChromeOver)) {
                isMainFrameNeeded = true;
            }
        }
        return isMainFrameNeeded;
    }

    /**
     * @brief Draw the active content surface's captured texture into the main
     * window back buffer (Wayland composite mode). Opaque (no blend).
     */
    void draw(Compositor & compositor)
    {
        auto it = m_surfaces.find(m_activeId);
        if (it == m_surfaces.end()) {
            return;
        }
        content_surface_t & entry = it->second;
        if (entry.composite.pixels.empty() || entry.composite.w <= 0 || entry.composite.h <= 0) {
            return;
        }
        compositor.draw(entry.pairing.get().window().bound(), false, entry.composite);
    }

    // Every composite texture let go of, in the main window's context, and the registry
    // dropped: the surfaces are the host's, already gone or about to be torn down by it
    void clear()
    {
        for (auto & [id, entry] : m_surfaces) {
            entry.composite.destroy();
        }
        m_surfaces.clear();
    }

private:
    const std::unique_ptr<MainConnector> &      m_main;
    const Geometry &                            m_geometry;
    PointerCursor &                             m_cursor;
    Ui::Window::RenderQueue &                   m_renderQueue;
    std::unordered_map<id_t, content_surface_t> m_surfaces;
    id_t                                        m_activeId  = Ui::INVALID_ID;
    id_t                                        m_pointerId = Ui::INVALID_ID; // the surface the pointer is over

    // Read a content surface's framebuffer into its composite cache (Wayland
    // composite path). Generic raw read of the just-rendered surface -
    // bottom-up, matching the FlipY draw in Compositor::draw.
    static void capture(content_surface_t & entry)
    {
        Ui::IWindow & content = entry.pairing.get().window();
        content.makeCurrent();
        entry.composite.capture(content.bound());
    }

    // Place a content surface at the current viewport rect. On Wayland the X11
    // child is parked offscreen (avoids surface flicker) but keeps its logical
    // position for hit testing.
    void position(Ui::IWindow & window)
    {
        const Ui::Res::Type::bound_t vp = m_geometry.viewport();
        if (g_config.isCompositing) {
            window.moveResize({ .x = -vp.w, .y = -vp.h, .w = vp.w, .h = vp.h });
            window.setPosition(vp.x, vp.y);
        } else {
            window.moveResize(vp);
        }
        // A Wayland subsurface takes its place with the main window's next commit, which only a
        // frame of its own brings
        m_renderQueue.request(MAIN_WINDOW_ID);
    }

    // `entry` moved and its pairing sized to the viewport: moveResize (+ the offscreen
    // fork) updates position, size and the cached bound() event routing relies on at once
    void fit(content_surface_t & entry)
    {
        Pairing & surface = entry.pairing;
        position(surface.window());
        const Ui::Res::Type::bound_t viewport = m_geometry.viewport();
        surface.resize(viewport.w, viewport.h);
    }
};

} // namespace Ui::Window
