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
#include "common/time.h"
#include "ui/config.h"
#include "ui/gl/localglew.h"
#include "ui/gl/svgrenderer.h"
#include "ui/idkind.h"
#include "ui/pubsub/subscribe.h"
#include "ui/window/compositor.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/tooltipconnector.h"
#include "ui/window/geometry.h"
#include "ui/window/hostsurfaces.h"
#include "ui/window/logpopupframe.h"
#include "ui/window/popupslot.h"
#include "ui/window/popupslots.h"
#include "ui/window/renderqueue.h"
#include "ui/window/tooltippopup.h"
#include "ui/window/underlay.h"

#include <iostream>
#include <memory>

namespace Ui::Window {

// The main window's frame, every popup-type window composited into it, and each popup
// brought up and drawn on its own: the one place the main window is drawn and presented,
// so a popup's corners are always captured off a frame drawn the same way
class MainFrame final : private Common::NonCopyable {
public:
    MainFrame(const std::unique_ptr<MainConnector> & main,
              const Geometry &                       geometry,
              Compositor &                           compositor,
              HostSurfaces &                         hostSurfaces,
              Underlay &                             underlay,
              TooltipPopup &                         tooltipPopup,
              popup_slots_t &                        slots,
              Ui::PubSub::Subscribe &                subscribe,
              Ui::Window::RenderQueue &              renderQueue)
        : m_main(main)
        , m_geometry(geometry)
        , m_compositor(compositor)
        , m_hostSurfaces(hostSurfaces)
        , m_underlay(underlay)
        , m_tooltipPopup(tooltipPopup)
        , m_slots(slots)
        , m_subscribe(subscribe)
        , m_renderQueue(renderQueue)
    {
    }

    /**
     * @brief Render all windows in the render queue, in correct GL context order.
     *
     * Drains the Subscribe render queue. Order:
     * 1. Main window (UI + content surface composites)
     * 2. Corner recapture + popup re-render (if popup in queue)
     * 3. Popup composite on main window
     * 4. Swap main window
     */
    void renderQueued()
    {
        const auto pending = m_renderQueue.pending();
        if (pending.empty()) {
            return;
        }

        // Clear before rendering so requests added during rendering survive
        m_renderQueue.clear();
        [[maybe_unused]] const auto started = Common::Time::now();

        // Only this pass's recaptures count in 2, not one a popup's bring-up made before it
        m_slots.forEach([](auto & slot) { slot.isRecaptured = false; });

        // 1. Render main window. Content-changed subscribers notified between the render
        // and the swap, outside renderMain: a GL context switch during an active render
        // corrupts GL state
        const bool isMainQueued = pending.contains(MAIN_WINDOW_ID);
        if (isMainQueued) {
            renderMain(true);
            m_subscribe.notify(MAIN_WINDOW_ID);
            drawComposites();
            swap();
        }

        // 2. Each popup-type window with a frame of its own due. Composited, the main pass
        // just rendered, read and drew in each one its recapture took (the notify above,
        // popup_slot_t::isRecaptured): a frame of their own would present the same picture
        // again, with a main render and swap each (renderAndSwap)
        m_slots.forEach([this, &pending](auto & slot) {
            if (slot.isRecaptured || !isUp(slot) || !pending.contains(slot.connector->window().subscribeId())) {
                return;
            }
            renderPopup(slot);
            markPresented(slot);
        });
        if constexpr (TOOLTIP_DEBUG) {
            if (m_tooltipPopup.isWanted()) {
                std::cout << "[WindowManager] frame with a tooltip in " << Common::Time::msSince(started)
                          << " ms (main " << isMainQueued << ")" << std::endl;
            }
        }
    }

    // The main window drawn and presented with every composite drawn in. Does NOT notify
    // subscribers: a popup's own frame redraws main with it this way, and a recapture
    // there would loop
    void renderAndSwap()
    {
        renderMain(true);
        drawComposites();
        swap();
    }

    // A popup-type window just made or changed, recaptured off a fresh main frame, every
    // popup drawn in before its swap - composited, that is it on screen
    template <typename TConnector>
    void recapture(popup_slot_t<TConnector> & slot)
    {
        renderMain(true);
        m_underlay.recapture(slot);
        drawComposites();
        swap();
    }

    // A popup-type window just made, brought up: recaptured on the main frame, and where
    // not composited its own first frame presented before it shows, so it is never flat
    template <typename TConnector>
    void present(popup_slot_t<TConnector> & slot)
    {
        recapture(slot);
        if (!g_config.isCompositing) {
            if (slot.connector->pairing().render()) {
                slot.connector->window().swapBuffers();
            }
            slot.connector->window().show();
        }
        m_main->window().makeCurrent();
    }

    // The main window's GL surface flushed (makeCurrent + swap) for an EGL surface resize
    void swapAfterResize()
    {
        // X11's EGL surface takes a new size at its next swap. A wl_egl_window has it already, and
        // swapping the frame drawn before it copies the old size into the new buffer
#ifndef HAVE_WAYLAND
        if (m_main) {
            m_main->window().makeCurrent();
            m_main->window().swapBuffers();
        }
#endif
    }

private:
    const std::unique_ptr<MainConnector> & m_main;
    const Geometry &                       m_geometry;
    Compositor &                           m_compositor;
    HostSurfaces &                         m_hostSurfaces;
    Underlay &                             m_underlay;
    TooltipPopup &                         m_tooltipPopup;
    popup_slots_t &                        m_slots;
    Ui::PubSub::Subscribe &                m_subscribe;
    Ui::Window::RenderQueue &              m_renderQueue;

    /**
     * @brief Render main window content to back buffer (no swap).
     * @param fullRedraw If true, clear framebuffer before rendering (needed for resize, theme switch, etc.)
     *
     * swap() presents it after. This split allows reading from the back buffer
     * (e.g. corner capture for popups) between render and swap.
     */
    void renderMain(bool fullRedraw)
    {
        if (m_main == nullptr || !m_main->hasRenderer()) {
            return;
        }
        m_main->window().makeCurrent();

        if (fullRedraw) {
            m_main->window().clear();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
        m_main->renderer().Render(m_geometry.width(), m_geometry.height());

        // In composite mode, draw content surfaces as textures (no popup yet --
        // popup composite is drawn after corner capture to avoid ghost artifacts)
        if (g_config.isCompositing) {
            m_hostSurfaces.draw(m_compositor);
        }
    }

    /**
     * @brief Draw popup composite texture on the main window back buffer.
     *
     * Call after the content-changed notify and before swap()
     * so corner capture reads UI + content surface only (no popup ghost).
     */
    void drawComposites()
    {
        if (!g_config.isCompositing) {
            return;
        }
        m_slots.forEach([this](auto & slot) {
            if (isUp(slot) && !slot.composite.pixels.empty()) {
                m_compositor.draw(slot.connector->window().bound(), true, slot.composite);
            }
        });
    }

    void swap()
    {
        if (m_main) {
            m_main->window().swapBuffers();
        }
    }

    // A popup-type window's own frame, then presented (composite capture on Wayland, swap
    // otherwise), the main context made current again
    template <typename TConnector>
    void renderPopup(popup_slot_t<TConnector> & slot)
    {
        TConnector & connector = *slot.connector;
        connector.pairing().render(); // makes its context current, then draws
        logPopupFrame(connector.window().bound());
        if (g_config.isCompositing) {
            slot.composite.capture(connector.window().bound());
            renderAndSwap();
        } else {
            connector.window().swapBuffers();
        }
        m_main->window().makeCurrent();
    }

    // Whether a popup-type window stands on the main window: made, and the tooltip wanted -
    // one kept hidden for the next is drawn nowhere, or composited it would be drawn in again
    template <typename TConnector>
    [[nodiscard]] bool isUp(const popup_slot_t<TConnector> & slot) const
    {
        return slot.connector != nullptr;
    }

    [[nodiscard]] bool isUp(const popup_slot_t<TooltipConnector> & slot) const
    {
        return slot.connector != nullptr && m_tooltipPopup.isWanted();
    }

    // A popup-type window's own frame presented: only the tooltip, shown once its first is,
    // waits for one
    template <typename TConnector>
    void markPresented(popup_slot_t<TConnector> & /*slot*/)
    {
    }

    void markPresented(popup_slot_t<TooltipConnector> & /*slot*/) { m_tooltipPopup.markPresented(); }
};

} // namespace Ui::Window
