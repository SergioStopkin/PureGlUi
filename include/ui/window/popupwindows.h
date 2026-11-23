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
#include "ui/pubsub/subscribe.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/eventfactory.h"
#include "ui/window/hostsurfaces.h"
#include "ui/window/popupslot.h"
#include "ui/window/renderqueue.h"

#include <memory>
#include <utility>

namespace Ui::Window {

// How every popup-type window over the main window - a menu, its submenu, the dialog, the
// Ctrl+Tab list and the tooltip - is made, given its renderer and closed, in the slot the
// WindowManager owns it in: each with an id of its own, known to the event handler, its
// frame asked for through the render queue and its `recapture` run on every main frame
class PopupWindows final : private Common::NonCopyable {
public:
    // All the window manager's, `main` and `events` empty until it makes them
    PopupWindows(const Ui::Res::ResManager &            resManager,
                 const std::unique_ptr<MainConnector> & main,
                 const std::unique_ptr<NativeEvent> &   events,
                 const HostSurfaces &                   hostSurfaces,
                 Ui::PubSub::Subscribe &                subscribe,
                 Ui::Window::RenderQueue &              renderQueue)
        : m_resManager(resManager)
        , m_main(main)
        , m_events(events)
        , m_hostSurfaces(hostSurfaces)
        , m_subscribe(subscribe)
        , m_renderQueue(renderQueue)
    {
    }

    // Makes a popup-type window at `bound` (main-window physical px) into `slot`: `recapture`
    // runs on every main frame, which a content surface redrawing under it asks for too
    // (renderContentSurfaces)
    template <typename TConnector, typename Fn>
    bool make(const Ui::Res::Type::bound_t &  bound,
              const Ui::Res::Type::border_t & radii,
              const Ui::Color &               background,
              bool                            isInputTransparent,
              Fn &&                           recapture,
              popup_slot_t<TConnector> &      slot)
    {
        slot.connector = std::make_unique<TConnector>(m_subscribe, nextId());
        auto & window  = slot.connector->window();
        window.setPosition(bound.x, bound.y);
        window.setBackground(background);
        window.setInputTransparent(isInputTransparent);
        if (radii.anyNonZero()) {
            window.setCornerRadii(radii);
        }
        if (!window.create(m_main->window(), bound.w, bound.h)) {
            slot.connector.reset();
            return false;
        }
        if (m_events) {
            m_events->addPopupWindow(window.nativeHandle());
        }
        // On Wayland, move popup offscreen to prevent XWayland surface flicker.
        // Keep it mapped for valid EGL rendering
        if (g_config.isCompositing) {
            window.move(-bound.w, -bound.h);
            window.setPosition(bound.x, bound.y);
        }
        window.setRenderRequest([this, &slot]() { m_renderQueue.request(slot.connector->window().subscribeId()); });
        m_subscribe.add(MAIN_WINDOW_ID, window.subscribeId(), std::forward<Fn>(recapture));
        return true;
    }

    // The renderer of the window make() made in `slot`, built in its context from the res and
    // `args`, sized to the window and told whether it is see-through
    template <typename TConnector, typename... Args>
    auto & emplaceRenderer(popup_slot_t<TConnector> & slot, Args &&... args)
    {
        auto & window   = slot.connector->window();
        auto & renderer = slot.connector->emplaceRenderer([&window] { window.makeCurrent(); },
                                                          m_resManager,
                                                          std::forward<Args>(args)...);
        renderer.resize(window.bound().w, window.bound().h);
        renderer.setAlpha(window.hasAlpha());
        return renderer;
    }

    // Closes what make() made: unregistered from the event handler, and the surface under
    // it asked to draw what it covered, before reset(); the main window's context current
    // again after, which a window's teardown may have left another's
    template <typename TConnector>
    void close(popup_slot_t<TConnector> & slot)
    {
        if (!slot.connector) {
            return;
        }
        if (m_events) {
            m_events->removePopupWindow(slot.connector->window().nativeHandle());
        }
        m_hostSurfaces.redrawUnder(slot.connector->window().bound());
        slot.connector.reset();
        slot.composite.clear();
        if (m_main) {
            m_main->window().makeCurrent();
        }
    }

private:
    const Ui::Res::ResManager &            m_resManager;
    const std::unique_ptr<MainConnector> & m_main;
    const std::unique_ptr<NativeEvent> &   m_events;
    const HostSurfaces &                   m_hostSurfaces;
    Ui::PubSub::Subscribe &                m_subscribe;
    Ui::Window::RenderQueue &              m_renderQueue;
    id_t                                   m_serial = 0; // the latest popup's (nextId)

    // A new id per open, never handed back (Ui::IdKind)
    [[nodiscard]] id_t nextId()
    {
        ++m_serial;
        return Ui::idOf(Ui::IdKind::Popup, m_serial);
    }
};

} // namespace Ui::Window
