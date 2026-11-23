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
#include "ui/config.h"
#include "ui/pubsub/subscribe.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/dialogaction.h"
#include "ui/type.h"
#include "ui/window/connector/dialogconnector.h"
#include "ui/window/dialogareaof.h"
#include "ui/window/geometry.h"
#include "ui/window/mainframe.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popupslot.h"
#include "ui/window/popupwindows.h"
#include "ui/window/renderqueue.h"
#include "ui/window/resolveddialogof.h"
#include "ui/window/underlay.h"

#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace Ui::Window {

// The modal dialog: its window made centred in the area inside the chrome, brought up,
// keyed, recentred on a resize and closed - what it closed with kept for the host's close
// hook. The window is the WindowManager's, a slot it hands in, with what the pointer does
// once the dialog is gone (`rehover`)
class DialogPopup final : private Common::NonCopyable {
public:
    DialogPopup(const Ui::Res::ResManager &     resManager,
                const Geometry &                geometry,
                PopupWindows &                  popups,
                Underlay &                      underlay,
                MainFrame &                     mainFrame,
                popup_slot_t<DialogConnector> & slot,
                Ui::PubSub::Subscribe &         subscribe,
                Ui::Window::RenderQueue &       renderQueue,
                Ui::task_fn_t                   rehover)
        : m_resManager(resManager)
        , m_geometry(geometry)
        , m_popups(popups)
        , m_underlay(underlay)
        , m_mainFrame(mainFrame)
        , m_slot(slot)
        , m_subscribe(subscribe)
        , m_renderQueue(renderQueue)
        , m_rehover(std::move(rehover))
    {
    }

    // Host callback fired when a dialog closes (the host reads lastAction/lastData).
    void setOnClose(Ui::task_fn_t callback) { m_onClose = std::move(callback); }

    // Host resolver for dialog content placeholders (%VERSION%/%CPU%/... ->
    // runtime values). The fw stays content-agnostic; if unset, the locale
    // string is shown verbatim.
    void setContentResolver(std::function<std::wstring(const std::string &)> resolver)
    {
        m_contentResolver = std::move(resolver);
    }

    // Where a dialog may sit, CSS px (dialogAreaOf)
    [[nodiscard]] Ui::Res::Type::bound_t area() const
    {
        return dialogAreaOf(m_resManager.layout(), m_geometry.width(), m_geometry.height());
    }

    void open(const Ui::Res::Type::dialog_t & dialog)
    {
        close();

        m_lastData = dialog;

        const Ui::Res::Type::dialog_t resolved = resolvedDialogOf(dialog,
                                                                  m_resManager.localeManager(),
                                                                  m_contentResolver);

        // Centred in the area inside the chrome, at its own size or res's
        const auto &                 layout = m_resManager.layout().dialog;
        const Ui::Res::Type::bound_t bound  = Ui::Window::Popup::DialogWindow::centeredBoundOf(
            area(),
            resolved.width > 0 ? resolved.width : layout.width,
            resolved.height > 0 ? resolved.height : layout.height);
        if (!m_popups.make(
                bound,
                layout.border.scaled(g_config.scale),
                m_resManager.theme().dialog.bg,
                false,
                // Only once recentred: every main frame would read what stands under it,
                // the host's view included, and draw it again whole
                [this]() {
                    if (m_isMoved) {
                        m_isMoved = false;
                        m_underlay.recapture(m_slot);
                    }
                },
                m_slot)) {
            std::cerr << "[WindowManager] Failed to create the dialog window" << std::endl;
            return;
        }
        std::cout << "[WindowManager] Opened dialog type=" << static_cast<int>(dialog.type) << std::endl;

        auto & dialogRenderer = m_popups.emplaceRenderer(m_slot, resolved);
        dialogRenderer.setCloseCallback([this, resolved](Ui::Res::Type::DialogAction result) {
            m_lastResult = result;
            m_lastData   = resolved;
            m_subscribe.defer([this]() {
                if (m_onClose) {
                    m_onClose();
                }
                close();
                m_rehover();
            });
        });
        // Wire smooth scroll animation to render queue
        dialogRenderer.setRenderRequest([this]() { m_renderQueue.request(m_slot.connector->window().subscribeId()); });

        // Its content in its back buffer before it shows: an empty borderless NSWindow
        // flashes for a frame otherwise (macOS orderFront is synchronous)
        m_mainFrame.present(m_slot);

        // Dialogs have no hover/mouse activity to piggy-back on after open, so the first
        // render+swap above can land on a drawable that is not yet attached to a visible
        // window (macOS NSOpenGLView - Platform::MacOsWindow::show binds the drawable on first show).
        // Queue one more frame so renderAll() repaints onto the now-bound surface.
        m_renderQueue.request(m_slot.connector->window().subscribeId());
    }

    void close()
    {
        if (m_slot.connector) {
            m_popups.close(m_slot);
            m_isMoved = false; // its corners gone with it, the next one read as it opens
            m_mainFrame.renderAndSwap();
            std::cout << "[WindowManager] Dialog closed" << std::endl;
        }
    }

    [[nodiscard]] bool isOpen() const { return m_slot.connector && m_slot.connector->window().isValid(); }

    // The dialog's window, nullptr with none
    [[nodiscard]] Ui::Window::Popup::DialogWindow * window() const
    {
        return m_slot.connector ? &m_slot.connector->window() : nullptr;
    }

    [[nodiscard]] Ui::Res::Type::DialogAction     lastAction() const { return m_lastResult; }
    [[nodiscard]] const Ui::Res::Type::dialog_t & lastData() const { return m_lastData; }

    void keyPress(const std::string & key) const
    {
        if (m_slot.connector && m_slot.connector->hasRenderer()) {
            m_slot.connector->renderer().onKeyPress(key);
        }
    }

    // Recentred in the area left after a resize: position only - the next main frame
    // recaptures its corners
    void recenter()
    {
        if (m_slot.connector) {
            m_slot.connector->window().recenter(area());
            m_isMoved = true;
        }
    }

private:
    const Ui::Res::ResManager &     m_resManager;
    const Geometry &                m_geometry;
    PopupWindows &                  m_popups;
    Underlay &                      m_underlay;
    MainFrame &                     m_mainFrame;
    popup_slot_t<DialogConnector> & m_slot;
    Ui::PubSub::Subscribe &         m_subscribe;
    Ui::Window::RenderQueue &       m_renderQueue;
    Ui::task_fn_t                   m_rehover; // the main window's hover caught up with the pointer, once closed
    Ui::task_fn_t                   m_onClose;
    std::function<std::wstring(const std::string &)> m_contentResolver;
    Ui::Res::Type::DialogAction m_lastResult = Ui::Res::Type::DialogAction::None; // what the dialog closed with
    Ui::Res::Type::dialog_t     m_lastData;                                       // and on what
    bool m_isMoved = false; // recentred since its corners were read, read again on the next main frame
};

} // namespace Ui::Window
