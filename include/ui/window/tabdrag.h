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
#include "ui/idkind.h"
#include "ui/render/dragtrack.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/renderqueue.h"
#include "ui/window/tabmoveof.h"

#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace Ui::Window {

// A tab a left press holds: a click until the pointer travels the click threshold, then a
// drag - the tab drawn under the pointer, kept along the strip, and each jump past a
// neighbour reported to the host
class TabDrag final : private Common::NonCopyable {
public:
    // All the window manager's, `main` empty until it makes the window
    TabDrag(const Ui::Res::ResManager &            resManager,
            const std::unique_ptr<MainConnector> & main,
            Ui::Window::RenderQueue &              renderQueue)
        : m_resManager(resManager)
        , m_main(main)
        , m_renderQueue(renderQueue)
    {
    }

    // A tab dragged along the strip moved: the tab, and the one it now stands right before
    // (INVALID_ID the end), the host's to apply and project back. A stream like a slider's,
    // a call per jump, so a hook rather than an intent
    void setOnTabMoved(std::function<void(id_t, id_t)> callback) { m_onTabMoved = std::move(callback); }

    // A left press on `tab`, at `cssX`: held, still a click
    void press(const Ui::Render::UiElement & tab, fpx_t cssX)
    {
        m_tabId = tab.id;
        m_track.begin(cssX, tab.bound.x, 1.0F);
        m_isMoving = false;
    }

    // A held tab on the move: nothing while the pointer is short of the click threshold -
    // still a click, hover going on as usual - then a drag, the pointer taken as a dock
    // drag takes it, the tab drawn under it, and a jump reported once the tab is drawn
    // animation.json's `swap-at` of the way over a neighbour. Whether it took the move
    bool move(fpx_t cssX)
    {
        if (m_tabId == Ui::INVALID_ID) {
            return false;
        }
        if (!m_isMoving) {
            if (m_track.travelFrom(cssX) < m_resManager.layout().dockDefaults.clickThreshold) {
                return false;
            }
            m_isMoving = true;
            // No longer a click: the tab shows pressed no more
            if (m_main && m_main->pairing().onMouseLeave()) {
                m_renderQueue.request(MAIN_WINDOW_ID);
            }
        }
        // Where the drag draws the tab, before the renderer holds it to the strip
        const fpx_t left        = m_track.valueAt(cssX);
        const bool  hasRenderer = m_main && m_main->hasRenderer();
        if (hasRenderer) {
            m_main->renderer().holdTab(m_tabId, left);
            m_renderQueue.request(MAIN_WINDOW_ID);
        }
        const std::optional<id_t> beforeId = Ui::Window::tabMoveOf(
            m_resManager.tabBar(),
            [this, hasRenderer](id_t id) {
                return hasRenderer ? m_main->renderer().bound(id) : Ui::Res::Type::bound_t {};
            },
            m_tabId,
            left,
            m_resManager.tabSwapAt());
        if (beforeId.has_value() && m_onTabMoved) {
            m_onTabMoved(m_tabId, beforeId.value());
        }
        return true;
    }

    // A held tab let go, its release seen or not: dragged, it glides from where it was
    // drawn to its place. Whether it had been dragged - short of that it was a click
    bool drop()
    {
        const bool wasMoving = m_isMoving;
        m_tabId              = Ui::INVALID_ID;
        m_isMoving           = false;
        if (wasMoving && m_main && m_main->hasRenderer()) {
            m_main->renderer().releaseTab();
            m_renderQueue.request(MAIN_WINDOW_ID);
        }
        return wasMoving;
    }

private:
    const Ui::Res::ResManager &            m_resManager;
    const std::unique_ptr<MainConnector> & m_main;
    Ui::Window::RenderQueue &              m_renderQueue;
    std::function<void(id_t, id_t)>        m_onTabMoved;
    Ui::Render::drag_track_t               m_track;                     // where it was pressed, along the strip
    id_t                                   m_tabId    = Ui::INVALID_ID; // the tab held
    bool                                   m_isMoving = false;          // past the threshold: its release is no click
};

} // namespace Ui::Window
