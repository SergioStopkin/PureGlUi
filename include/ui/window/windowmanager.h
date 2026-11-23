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
#include "common/system.h"
#include "ui/config.h"
#include "ui/const.h"
#include "ui/gl/glrender.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/idkind.h"
#include "ui/interface/ieventapp.h"
#include "ui/interface/iwindow.h"
#include "ui/pubsub/subscribe.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/elementevent.h"
#include "ui/render/eventkind.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/row.h"
#include "ui/res/key/iconrole.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/changed.h"
#include "ui/type.h"
#include "ui/window/compositor.h"
#include "ui/window/connector/dialogconnector.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/menuconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/connector/tabswitcherconnector.h"
#include "ui/window/contenthit.h"
#include "ui/window/dialogpopup.h"
#include "ui/window/display.h"
#include "ui/window/displaystextof.h"
#include "ui/window/dockaction.h"
#include "ui/window/dockdrag.h"
#include "ui/window/docks.h"
#include "ui/window/event.h"
#include "ui/window/eventfactory.h"
#include "ui/window/eventtype.h"
#include "ui/window/geometry.h"
#include "ui/window/hostsurfaces.h"
#include "ui/window/ismouseevent.h"
#include "ui/window/mainframe.h"
#include "ui/window/matchpopuptarget.h"
#include "ui/window/menupopups.h"
#include "ui/window/mousebutton.h"
#include "ui/window/nativewindow.h"
#include "ui/window/nativewindowhandle.h"
#include "ui/window/pointercursor.h"
#include "ui/window/pointershape.h"
#include "ui/window/popupslots.h"
#include "ui/window/popupwindows.h"
#include "ui/window/refreshrateof.h"
#include "ui/window/renderqueue.h"
#include "ui/window/scalefactorof.h"
#include "ui/window/tabdrag.h"
#include "ui/window/tabswitcherpopup.h"
#include "ui/window/tooltippopup.h"
#include "ui/window/underlay.h"
#include "ui/window/windowmanagerdebug.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui::Window {

// Windowing primitives + event types now live in the framework backend.
using Ui::Window::Event;
using Ui::Window::EventFactory;
using Ui::Window::EventType;
using Ui::Window::MouseButton;
using Ui::Window::NativeEvent;
using Ui::Window::NativeWindow;
using Ui::Window::NativeWindowHandle;

/**
 * @brief Manages the main window and all child window contexts
 *
 * The WindowManager runs on the main thread and handles:
 * - Main window management
 * - Creating/destroying child window contexts
 * - Dispatching events to appropriate child windows
 * - Window layout management (UI margins)
 */
class WindowManager final : public Ui::IEventApp, private Common::NonCopyable {
    std::atomic<bool> m_isRunning { false };
    // Sticky, and not derivable from m_isRunning: that is already false before init,
    // so a stop arriving during initialize() would leave no trace and be undone by
    // the m_isRunning = true at the end of it. A signal caught mid-startup has to
    // still stop the app.
    std::atomic<bool> m_isStopRequested { false };

    std::unique_ptr<MainConnector> m_main;
    const Ui::Res::ResManager &    m_resManager; // non-owning; lifetime guaranteed by the owner (Shell/host)

    int m_pointerX = Ui::NO_POINTER; // where the pointer is, main-window physical px, whichever window has it
    int m_pointerY = Ui::NO_POINTER;

    Geometry   m_geometry;                  // the window's size and the UI margins round the viewport
    Compositor m_compositor { m_geometry }; // draws content/popup textures onto main window

    // Built in initialize() from m_resManager.layout().docks and rebuilt in apply() on
    // Ui::Res::Type::Changed::Layout (resource reload picks up new or removed dock files)
    Docks    m_docks { m_resManager, m_geometry };
    DockDrag m_dockDrag { m_docks }; // a dock's grip, slider or scrollbar holding the pointer

    // Platform-specific event handler (compile-time detected)
    std::unique_ptr<NativeEvent> m_eventHandler;
    PointerCursor                m_cursor { m_main, m_eventHandler }; // what the pointer shows

    // Content-changed subscriber system
    Ui::PubSub::Subscribe   m_subscribe;
    Ui::Window::RenderQueue m_renderQueue;

    TabDrag m_tabDrag { m_resManager, m_main, m_renderQueue }; // a tab a left press holds

    // The host's content surfaces, the active one shown (HostSurfaces)
    HostSurfaces m_hostSurfaces { m_main, m_geometry, m_cursor, m_renderQueue };

    // Popup-type windows made and closed (PopupWindows), and what stands under each read
    // for its corners (Underlay)
    PopupWindows m_popups { m_resManager, m_main, m_eventHandler, m_hostSurfaces, m_subscribe, m_renderQueue };
    Underlay     m_underlay { m_main, m_geometry, m_hostSurfaces };

    // Every popup-type window, owned here (popup_slots_t)
    popup_slots_t m_slots;

    // The tooltip's behaviour, opened and closed after every event and frame in its slot
    TooltipPopup m_tooltipPopup { m_resManager, m_main,     m_geometry,      m_hostSurfaces,
                                  m_popups,     m_underlay, m_slots.tooltip, m_renderQueue };

    // The main window's frame and every popup brought up and drawn (MainFrame)
    MainFrame m_mainFrame { m_main,         m_geometry, m_compositor, m_hostSurfaces, m_underlay,
                            m_tooltipPopup, m_slots,    m_subscribe,  m_renderQueue };

    // Each popup kind, in its own slot: the menu and its submenu, the dialog and the Ctrl+Tab
    // list. A dialog or the list closed, the main window's hover catches up with the pointer
    MenuPopups       m_menus { m_resManager, m_main, m_popups, m_underlay, m_mainFrame, m_slots.menu, m_slots.submenu };
    DialogPopup      m_dialog { m_resManager, m_geometry,    m_popups,
                                m_underlay,   m_mainFrame,   m_slots.dialog,
                                m_subscribe,  m_renderQueue, [this]() { rehover(); } };
    TabSwitcherPopup m_tabSwitcher { m_resManager,
                                     m_main,
                                     m_geometry,
                                     m_pointerX,
                                     m_pointerY,
                                     m_popups,
                                     m_underlay,
                                     m_mainFrame,
                                     m_slots.tabSwitcher,
                                     m_renderQueue,
                                     [this]() { rehover(); } };

    // Content refresh: marks the main UI dirty for the next frame.
    bool m_contentIsDirty = false;

    // Event dispatch state
    Event                       m_currentEvent {};
    Ui::Render::element_event_t m_lastPressResult {}; // what a main-window press landed on
    Ui::Render::element_event_t m_lastClickResult {};
    bool                        m_popupEventConsumed = false;

    // Open, keep or close the tooltip (TooltipPopup::sync)
    void syncTooltip() { m_tooltipPopup.sync(isChromeOpen()); }

    // Where the pointer is, main-window physical px, from an event of any window before
    // routing rebases it: a popup, submenu, dialog, the Ctrl+Tab list or content surface stands on the main
    // window at its bound (composited, all of them already report main-window px). A
    // leave takes it off: the next event from whichever window has it puts it back
    void trackPointer(const Event & event)
    {
        if (event.type == EventType::MouseLeave) {
            m_pointerX = Ui::NO_POINTER;
            m_pointerY = Ui::NO_POINTER;
            return;
        }
        if (!isMouseEvent(event)) {
            return;
        }
        Ui::Res::Type::bound_t origin {};
        if (!g_config.isCompositing) {
            if (const auto content = contentWindow(event.childWindowId)) {
                origin = content->get().bound();
            }
            const auto standsOn = [&event, &origin](const auto & slot) {
                if (slot.connector && event.sourceWindow == slot.connector->window().nativeHandle()) {
                    origin = slot.connector->window().bound();
                }
            };
            m_slots.forEach(standsOn);
        }
        m_pointerX = event.mouse.x + static_cast<int>(origin.x);
        m_pointerY = event.mouse.y + static_cast<int>(origin.y);
    }

    // A menu or dialog is open, and no tooltip shows over it
    [[nodiscard]] bool isChromeOpen() const
    {
        return m_slots.menu.connector || m_slots.dialog.connector || m_slots.tabSwitcher.connector;
    }

    // A tooltip is wanted - presented or not yet (TooltipPopup::isWanted)
    [[nodiscard]] bool hasTooltip() const { return m_tooltipPopup.isWanted(); }

public:
    explicit WindowManager(const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
    {
    }

    ~WindowManager() override { shutdown(); }

    Ui::PubSub::Subscribe &   subscribe() { return m_subscribe; }
    Ui::Window::RenderQueue & renderQueue() { return m_renderQueue; }

    // The popup kinds, each over the windows this owns: the menu and its submenu, the modal
    // dialog and the Ctrl+Tab list's window
    [[nodiscard]] MenuPopups &             menus() { return m_menus; }
    [[nodiscard]] const MenuPopups &       menus() const { return m_menus; }
    [[nodiscard]] DialogPopup &            dialog() { return m_dialog; }
    [[nodiscard]] const DialogPopup &      dialog() const { return m_dialog; }
    [[nodiscard]] TabSwitcherPopup &       tabSwitcher() { return m_tabSwitcher; }
    [[nodiscard]] const TabSwitcherPopup & tabSwitcher() const { return m_tabSwitcher; }

    // The main window's GL surface flushed for an EGL surface resize (MainFrame::swapAfterResize)
    void swapAfterResize() { m_mainFrame.swapAfterResize(); }

    void drainDeferred() { m_subscribe.drainDeferred(); }

    /** @brief Host reaction to a slider row being dragged: (row id, 0..1) */
    void setOnRowValue(std::function<void(id_t, fpx_t)> callback) { m_dockDrag.setOnRowValue(std::move(callback)); }

    // A tab dragged along the strip moved: the tab, and the one it now stands right before
    // (INVALID_ID the end), the host's to apply and project back. A stream like a slider's,
    // a call per jump, so a hook rather than an intent
    void setOnTabMoved(std::function<void(id_t, id_t)> callback) { m_tabDrag.setOnTabMoved(std::move(callback)); }

    void requestRender(id_t windowId) { m_renderQueue.request(windowId); }
    void requestMainRender() { m_renderQueue.request(MAIN_WINDOW_ID); }

    // A glide or a fade the last frame drew midway asks for the next, once a pass of the run
    // loop - at the display's pace, and with nothing gliding, none. The main window's, and a
    // menu's, a submenu's, a dialog's or the Ctrl+Tab list's, each of which draws its own
    void keepGliding()
    {
        if (isMainGliding()) {
            requestMainRender();
        }
        const auto keepPopup = [this](const auto & slot) {
            if (slot.connector && slot.connector->hasRenderer() && slot.connector->renderer().isGliding()) {
                m_renderQueue.request(slot.connector->window().subscribeId());
            }
        };
        m_slots.forEach(keepPopup);
    }

    void requestContentRefresh()
    {
        m_contentIsDirty = true;
        requestMainRender();
    }

    // Whether the layout still has to be rebuilt before the next frame. Drawing
    // reads live geometry while hit rects come from the element list, so anything
    // that moves an element must leave this true.
    [[nodiscard]] bool isContentDirty() const { return m_contentIsDirty; }

    // Double-click thresholds (host supplies these from its viewport config).
    void setDoubleClickConfig(uint32_t intervalMs, int distancePx)
    {
        if (m_eventHandler) {
            m_eventHandler->setDoubleClickConfig(intervalMs, distancePx);
        }
    }

    // Pointer shape (PointerCursor)
    void setCursor(Ui::Window::PointerShape shape) { m_cursor.set(shape); }

    [[nodiscard]] Ui::Window::PointerShape pointerShape() const { return m_cursor.shape(); }

    // Project host content into a dock, addressed by its res JSON "name" (the
    // routing key; numeric dock ids are reassigned across a reload). Unknown
    // name is a no-op, so a host can set rows for a dock its res does not
    // declare without special-casing.
    void setDockRows(std::string_view dockName, std::vector<Ui::Res::Dock::row_t> rows)
    {
        for (const auto & row : rows) {
            if (row.id != Ui::INVALID_ID && !Ui::isHostId(row.id)) {
                std::cerr << "[WindowManager] Dock row id " << row.id << " carries no host kind" << std::endl;
            }
        }
        if (m_docks.setRows(dockName, std::move(rows))) {
            // Rows are drawn live but hit-tested from the element list, which
            // only setContent() rebuilds - a repaint alone would leave every
            // row clickable at the position it held before this call
            requestContentRefresh();
        }
    }

    // The res menu a dock Menu row names, empty when the element is not one (Docks)
    [[nodiscard]] std::string rowMenuKey(id_t elementId) const { return m_docks.menuKeyOf(elementId); }

    // Back to the top. setDockRows cannot decide this itself: it cannot tell "same
    // content, a node expanded" from "different content entirely", and only the
    // host knows which it just projected
    void resetDockScroll(std::string_view dockName)
    {
        if (m_docks.resetScroll(dockName)) {
            requestContentRefresh(); // same stale hit rects as setDockRows
        }
    }

    // The rect left for embedded content (Geometry::viewport)
    [[nodiscard]] Ui::Res::Type::bound_t viewportBound() const { return m_geometry.viewport(); }

    // ---- Content-surface registry (the host-facing seam, HostSurfaces) ----

    // Register a host-owned content surface by its connector's pairing, which must outlive the registration.
    // An id is added once: remove it before adding it again, or the first pairing stays registered
    void addContentSurface(id_t id, Pairing & pairing) { m_hostSurfaces.add(id, pairing); }

    void removeContentSurface(id_t id) { m_hostSurfaces.remove(id); }

    // Make one content surface active: only it is shown (the windows overlap).
    void setActiveContentSurface(id_t id) { m_hostSurfaces.setActive(id); }

    // Mark a content surface ready (or not) for rendering - cleared while a model loads
    void setContentSurfaceReady(id_t id, bool isReady) { m_hostSurfaces.setReady(id, isReady); }

    // What the pointer shows over a content surface - its host's to say
    void setContentPointer(id_t id, Ui::Window::PointerShape shape) { m_hostSurfaces.setPointer(id, shape); }

    // A tooltip for something a content surface's host marks - a point it snaps to,
    // say: `text` beside `anchor`, a rect in the surface's physical pixels, as the
    // toolbar's stands beside its button; an empty text for none. Shown while the
    // pointer is over that surface, once the event being dispatched or the frame
    // being drawn is done (syncTooltip); a menu or dialog open shows none
    void setContentTooltip(id_t id, const std::string & text, const Ui::Res::Type::bound_t & anchor)
    {
        m_hostSurfaces.setTooltip(id, text, anchor);
    }

    [[nodiscard]] bool isContentReady(id_t id) const { return m_hostSurfaces.isReady(id); }

    // Resolve a native window handle to its content-surface id, or INVALID_ID for
    // the main window / an unregistered handle. The single source the event peers'
    // child-window lookup is wired to (see initialize()).
    [[nodiscard]] id_t childIdForHandle(Ui::Window::NativeWindowHandle handle) const
    {
        return m_hostSurfaces.idOf(handle);
    }

    // A content surface's window and its pairing, none for an unknown id
    [[nodiscard]] std::optional<std::reference_wrapper<Ui::IWindow>> contentWindow(id_t id) const
    {
        return m_hostSurfaces.window(id);
    }

    [[nodiscard]] std::optional<std::reference_wrapper<Ui::IWindow>> activeContentWindow() const
    {
        return m_hostSurfaces.activeWindow();
    }

    [[nodiscard]] std::optional<std::reference_wrapper<Pairing>> contentPairing(id_t id) const
    {
        return m_hostSurfaces.pairing(id);
    }

    [[nodiscard]] std::optional<std::reference_wrapper<Pairing>> activeContentPairing() const
    {
        return m_hostSurfaces.activePairing();
    }

    [[nodiscard]] std::optional<content_hit_t> hitTestActiveContent(int x, int y) const
    {
        return m_hostSurfaces.hitTestActive(x, y);
    }

    [[nodiscard]] const Event &                       currentEvent() const { return m_currentEvent; }
    [[nodiscard]] const Ui::Render::element_event_t & lastPressResult() const { return m_lastPressResult; }
    [[nodiscard]] const Ui::Render::element_event_t & lastClickResult() const { return m_lastClickResult; }
    [[nodiscard]] bool                                popupEventConsumed() const { return m_popupEventConsumed; }

    /**
     * @brief Dispatch a platform event: store, route to windows, notify subscribers
     *
     * Handles:
     * 1. Composite mode popup coordinate rerouting
     * 2. Mechanical routing of events to popup / main / child windows
     * 3. Notifying subscribers via Ui::idOf(Ui::IdKind::Event, EventType)
     *
     * The shell subscribes to event notifications for policy decisions (menu
     * management, shortcuts, popup close logic).
     */
    void dispatchEvent(const Event & incoming)
    {
        m_lastPressResult    = {};
        m_lastClickResult    = {};
        m_popupEventConsumed = false;
        trackPointer(incoming);

        // Local mutable copy: classify/route rebase coords + set isPopupEvent.
        // Event is over-aligned (alignas(128)); MSVC (C2719) forbids that as a
        // by-value parameter but allows it as a local.
        Event event = incoming;

        // Dialog is modal - when one is open it is the only event target. It is a
        // different connector type than the popup/submenu, but classification is
        // the same (matchPopupTarget), so this stays a one-liner.
        if (m_slots.dialog.connector) {
            matchPopupTarget<DialogConnector>({ m_slots.dialog.connector.get() }, event);
            m_currentEvent = event;
            if (event.isPopupEvent) {
                routePopupEvent(event, *m_slots.dialog.connector);
            }
            m_subscribe.notify(Ui::idOf(Ui::IdKind::Event, event.type));
            syncTooltip();
            return;
        }

        // The Ctrl+Tab list, over everything but a dialog: its rows hovered and clicked, the
        // shell reading what they offered after this (TabSwitcherPopup::hoveredRow, the click)
        if (auto & list = m_slots.tabSwitcher; list.connector) {
            // Its scrollbar's thumb held keeps the pointer until the button comes up, as every
            // toolkit's does: each move and the release go to the list wherever the pointer is,
            // from whichever window, at the list's own px (trackPointer has it in the main window's)
            const bool isCaptured = event.type == EventType::MouseMove || event.type == EventType::MouseButtonRelease;
            if (isCaptured && m_tabSwitcher.isThumbHeld() && m_pointerX != Ui::NO_POINTER) {
                const Ui::Res::Type::bound_t & bound = list.connector->window().bound();
                event.mouse.x                        = m_pointerX - static_cast<int>(bound.x);
                event.mouse.y                        = m_pointerY - static_cast<int>(bound.y);
                event.isPopupEvent                   = true;
                m_currentEvent                       = event;
                if (event.type == EventType::MouseMove) {
                    if (list.connector->pairing().onMouseMove(event.mouse.x, event.mouse.y)) {
                        list.connector->window().requestRender();
                    }
                } else {
                    m_lastClickResult = list.connector->pairing().onMouseRelease(event.mouse.x,
                                                                                 event.mouse.y,
                                                                                 event.mouse.button);
                    list.connector->window().requestRender();
                }
                m_subscribe.notify(Ui::idOf(Ui::IdKind::Event, event.type));
                syncTooltip();
                return;
            }
            if (auto * hit = matchPopupTarget<TabSwitcherConnector>({ list.connector.get() }, event); hit != nullptr) {
                m_currentEvent = event;
                routePopupEvent(event, *hit);
                m_subscribe.notify(Ui::idOf(Ui::IdKind::Event, event.type));
                syncTooltip();
                return;
            }
            // A move off it hovers no row, or the shell hands the highlight back to the last one
            if (event.type == EventType::MouseMove) {
                list.connector->pairing().onMouseLeave();
            }
        }

        // Classify event source window.
        // In composite mode: popups are offscreen, classify by bounds.
        // In non-composite mode: classify by native window handle.
        // Popup and submenu are the same type, so a single MenuConnector* covers both.
        MenuConnector * popupTarget = classifyPopupEvent(event);

        m_currentEvent = event;

        const bool isChildEvent = (event.childWindowId != Ui::INVALID_ID);
        const bool isPopupEvent = event.isPopupEvent;

        // Route popup-type events (submenu, popup menu)
        if (isPopupEvent && popupTarget != nullptr) {
            routePopupEvent(event, *popupTarget);
        }

        // Route non-popup events to main/child windows
        if (!isPopupEvent) {
            switch (event.type) {
            case EventType::MouseMove:
                if (isChildEvent) {
                    onMouseMove(event.mouse.x, event.mouse.y, event.childWindowId);
                } else {
                    onMouseMove(event.mouse.x, event.mouse.y);
                }
                break;
            // Every button goes through: a content surface needs middle for pan
            // and right for a context menu. Chrome handlers filter for
            // themselves - the dispatcher no longer decides for them
            case EventType::MouseButtonPress:
                if (isChildEvent) {
                    onMousePress(event.mouse.x,
                                 event.mouse.y,
                                 event.childWindowId,
                                 event.mouse.button,
                                 event.mouse.clickCount,
                                 event.mouse.modifiers);
                } else {
                    m_lastPressResult = onMousePress(event.mouse.x,
                                                     event.mouse.y,
                                                     event.mouse.button,
                                                     event.mouse.clickCount,
                                                     event.mouse.modifiers);
                }
                break;
            case EventType::MouseButtonRelease:
                m_lastClickResult = onMouseRelease(event.mouse.x,
                                                   event.mouse.y,
                                                   isChildEvent ? event.childWindowId : Ui::INVALID_ID,
                                                   event.mouse.button);
                break;
            case EventType::Scroll:
                if (isChildEvent) {
                    onScroll(event.mouse.x, event.mouse.y, event.scroll.deltaY, event.childWindowId);
                } else {
                    onScroll(event.mouse.x, event.mouse.y, event.scroll.deltaY);
                }
                break;
            case EventType::MouseLeave: onMouseLeave(); break;
            default: break;
            }
        }

        // Notify subscribers
        m_subscribe.notify(Ui::idOf(Ui::IdKind::Event, event.type));

        // Last, after the shell has had its say: a menu it opened closes the tooltip
        syncTooltip();
    }

    /**
     * @brief Create the main window and supporting state. ResManager was
     * captured at construction; this just wires up GL / dock / event state
     * that needs the main window to exist first.
     */
    bool initialize()
    {
        // XWayland composite workaround flag (canonical home, read fw-wide via
        // g_config.isCompositing):
        // - WITH_WAYLAND=ON  -> false (native subsurfaces, no workaround)
        // - WITH_WAYLAND=OFF + Wayland session -> true (XWayland composite)
        // - WITH_WAYLAND=OFF + X11 session     -> false (normal child windows)
        // When true, content/popup X11 windows render offscreen and composite as
        // textures in the main window to dodge fullscreen direct-scanout issues.
#ifdef HAVE_WAYLAND
        g_config.isCompositing = false;
#else
        g_config.isCompositing = !Common::System::environmentVariable("WAYLAND_DISPLAY").empty();
#endif

        const auto & layout = m_resManager.layout();
        m_geometry.setSize(layout.windowWidth, layout.windowHeight);

        // Restore previously persisted window size if session.json carries it.
        // sessionWindowWidth/Height default to 0 when no prior run committed
        // geometry, in which case the layout defaults above are used as-is.
        const int sessionW = m_resManager.sessionWindowWidth();
        const int sessionH = m_resManager.sessionWindowHeight();
        if (sessionW > 0 && sessionH > 0) {
            m_geometry.setSize(static_cast<fpx_t>(sessionW), static_cast<fpx_t>(sessionH));
        }

        // Create main window
        m_main = std::make_unique<MainConnector>(m_subscribe, MAIN_WINDOW_ID);

        // Set background color before create() so window uses correct bg pixel
        m_main->window().setBackground(m_resManager.theme().second.bg);

        if (!m_main->window().create(m_geometry.width(), m_geometry.height(), nullptr, 0, m_resManager.title())) {
            std::cerr << "[WindowManager] Failed to create main window" << std::endl;
            return false;
        }

        // Update dimensions from actual window
        m_geometry.setSize(m_main->window().bound().w, m_main->window().bound().h);

        // Restore window position. create() doesn't accept x/y, so we apply
        // the saved position after mapping via moveResize. May cause a brief
        // flash from the WM's default position to the restored one; the
        // alternative (XMoveWindow before XMapWindow) requires plumbing x/y
        // through create() on every platform. No-op on Wayland - clients
        // can't position themselves there. Width/height in the moveResize
        // are the just-validated bound() values so we don't fight the WM
        // on its sizing decision.
        if (sessionW > 0 && sessionH > 0) {
            m_main->window().moveResize({ .x = static_cast<fpx_t>(m_resManager.sessionWindowX()),
                                          .y = static_cast<fpx_t>(m_resManager.sessionWindowY()),
                                          .w = m_geometry.width(),
                                          .h = m_geometry.height() });
        }
        // Ended fullscreen, started so: asked for, where a size alone gives none
        if (m_resManager.isSessionWindowFullscreen()) {
            m_main->window().setFullscreen(true);
        }

        // Wire render request to Subscribe render queue
        m_main->window().setRenderRequest([this]() { m_renderQueue.request(MAIN_WINDOW_ID); });

        applyWindowIcon();

        m_main->window().makeCurrent();

        // Load the GL entry points (GLEW off-macOS; no-op loader on macOS).
        if (!Ui::Gl::Util::initGlLoader()) {
            return false;
        }
        std::cout << "[WindowManager] GL loader initialized successfully" << std::endl;

        // The render pipeline is core-profile GL 3.3 (#version 330 shaders,
        // VAOs). On a legacy context (e.g. Microsoft's software GL 1.1 when no
        // GPU driver is installed) the modern entry points are NULL and the
        // first frame would segfault - fail here with a clear message instead.
        if (!Ui::Gl::Util::hasModernGl()) {
            std::cerr << "[WindowManager] OpenGL >= 3.3 required, got: " << Ui::Gl::Util::glString(GL_VERSION, "none")
                      << " - install a GPU driver or a software GL like Mesa (see mesa-windows.sh)" << std::endl;
            return false;
        }

        // Context is live: fold the GL_RENDERER software-rasterizer check into
        // isHardwareGl() so every consumer sees the true answer (e.g. Mesa
        // llvmpipe on a GPU-less machine, which create() cannot detect).
        m_main->window().refreshHardwareGl();

        // Instantiate one Ui::Render::DockColumn per parsed dock config. Keys are stable
        // (IdKind::Dock + index in the parsed array) so any Subscribe wiring that
        // happens later can address a dock by id. Initial state is pulled
        // from the session via ResManager::dockState(name) inside the ctor.
        m_docks.make();

        // Session-restored widths might overflow the current window (smaller
        // screen than last run, or hand-edited session.json): clamped now, before
        // the first layout/render pass
        m_docks.clampToWindow();

        refreshDisplayMetrics();

        // Initialize platform-specific event handler. Double-click thresholds
        // come from the host via setDoubleClickConfig() after initialize().
        m_eventHandler = EventFactory::create();
        m_eventHandler->init(m_main->window());

        // Child (content-surface) event routing: resolve a native handle to its
        // child id against the content-surface registry (the single owner). The
        // event peer calls this at poll time to stamp event.childWindowId. Wayland
        // content surfaces are driven via a separate X11 path, so leave it unwired
        // there (matches the prior per-surface registration's HAVE_WAYLAND guard).
#ifndef HAVE_WAYLAND
        m_eventHandler->setChildWindowLookup(
            [this](Ui::Window::NativeWindowHandle handle) { return childIdForHandle(handle); });
#endif

        std::cout << "[WindowManager] Main window initialized: " << m_main->window().nativeHandle() << " "
                  << m_geometry.width() << "x" << m_geometry.height() << std::endl;

        m_isRunning = true;
        return true;
    }

    // The DPI the UI is to be laid out at: the main window's, as its OS gives it, times
    // PUREGLUI_SCALE where that is set; g_config.dpi is the one it is laid out at now
    [[nodiscard]] int askedDpi() const
    {
        return roundToInt(static_cast<fpx_t>(m_main->window().dpi())
                          * scaleFactorOf(Common::System::environmentVariable(SCALE_VARIABLE)));
    }

    /**
     * @brief Re-query display DPI and recompute UI margins.
     *
     * Should be called whenever the display environment may have changed -
     * resource reload, monitor change, system zoom adjustment. Caller is
     * responsible for cascading the change (re-running the resize handler,
     * forcing a content rebuild) when the return value is true.
     *
     * @return true if g_config.scale actually changed.
     */
    bool refreshDisplayMetrics()
    {
        if (!m_main) {
            return false;
        }
        const float prevScale = g_config.scale;
        g_config.dpi          = askedDpi();
        g_config.scale        = static_cast<float>(g_config.dpi) / 96.0F;
        // 0 is no answer, which keeps the rate there was
        const std::vector<display_t> displays    = NativeWindow::displaysOf(m_main->window().nativeDisplay());
        const fpx_t                  refreshRate = refreshRateOf(displays);
        if (refreshRate > 0.0F) {
            g_config.refreshRate = refreshRate;
        }
        std::cout << "[WindowManager] Displays:\n" << displaysTextOf(displays) << std::endl;

        m_geometry.setChromeMargins(m_resManager.layout());

        // Dock widths add on top of the toolbar margins; layOut places
        // each dock's outer rect using the now-final viewport edges.
        applyDockMargins();
        m_docks.layOut();

        const bool scaleChanged = (prevScale != g_config.scale);
        // The tooltip's font and corners are physical px, made at the old scale
        if (scaleChanged) {
            m_tooltipPopup.destroy();
        }
        std::cout << "[WindowManager] Scale factor: " << g_config.scale << "x  refresh " << g_config.refreshRate
                  << " Hz  UI margins: L=" << m_geometry.left() << " T=" << m_geometry.top()
                  << " R=" << m_geometry.right() << " B=" << m_geometry.bottom() << " docks=" << m_docks.size()
                  << (scaleChanged ? " (changed)" : "") << std::endl;

        // Here, not at window creation: the floor is physical, so it can only be
        // computed once the scale this pass just resolved is known
        applyMinWindowSize();
        return scaleChanged;
    }

    // Add the current sum of dock widths into the side margins on top
    // of the toolbar widths. Called from refreshDisplayMetrics (which set
    // the toolbar baselines first) and from onDockStateCommitted (which
    // resets to toolbar widths then re-applies).
    void applyDockMargins()
    {
        m_docks.forEach([this](id_t /*id*/, const Ui::Render::DockColumn & dock) {
            m_geometry.addDock(dock.anchor(), dock.currentWidth());
        });
    }

    // Recompute dock-driven margins from scratch (toolbar baselines + current
    // dock widths including any transient drag width), reposition each dock's
    // outer rect, then push the new viewport bounds to each content surface. Used
    // every mouse-move during a grip drag (so the content surface tracks the
    // cursor live) and once at commit (collapse / expand / drag-release).
    // No session save here - Ui::Render::DockColumn writes session.json through
    // ResManager::setDockState whenever its committed state changes.
    void reflowDocksAndViewport()
    {
        if (!m_main) {
            return;
        }
        m_geometry.resetSideMargins(m_resManager.layout());
        applyDockMargins();
        m_docks.layOut();

        // Each queued to draw again - renderAll() draws content surfaces before the main
        // window, so the composite stays in step with the drag
        m_hostSurfaces.fitToViewport();

        // Hit rects live in the layout's element list, and only setContent()
        // rebuilds it. Without this a dock that just moved is DRAWN at its new
        // geometry - DockColumn::render reads m_outer live - while its grip stays
        // clickable at the OLD one: the first toggle works, every one after it
        // lands on empty space and does nothing.
        //
        // Skipped mid-drag: the pointer is captured then, so no hit test runs, and
        // DockDrag::end lets it go before the release reflows again.
        if (!m_dockDrag.isHolding()) {
            m_main->renderer().setContent();
        }
        m_renderQueue.request(MAIN_WINDOW_ID);
    }

    // Set the main-window icon from icon-defaults.json roles. The symbolic icon
    // is optional - pass an empty path so the platform layer skips it rather than
    // trying to load the icon directory as an SVG.
    void applyWindowIcon()
    {
        if (!m_main) {
            return;
        }
        const std::string mainIcon = m_resManager.iconDefault(Ui::Res::Key::IconRoleKey::WindowIcon).icon;
        if (mainIcon.empty()) {
            return;
        }
        const std::string symbolicIcon = m_resManager.iconDefault(Ui::Res::Key::IconRoleKey::WindowIconSymbolic).icon;
        const auto &      resPath      = m_resManager.resPath();
        m_main->window().setWindowIcon(resPath.icon(mainIcon),
                                       symbolicIcon.empty() ? std::string {} : resPath.icon(symbolicIcon));
    }

    // Push the res-derived floor to the window manager. Re-applied on reload, so
    // a res edit that adds a bigger dialog raises it without a restart.
    void applyMinWindowSize()
    {
        if (!m_main) {
            return;
        }
        const Ui::Res::Type::bound_t minSize = m_resManager.minWindowSize();
        const fpx_t                  minW    = toPhys(minSize.w);
        const fpx_t                  minH    = toPhys(minSize.h);
        m_main->window().setMinSize(minW, minH);
        const fpx_t width  = m_geometry.width();
        const fpx_t height = m_geometry.height();
        std::cout << "[WindowManager] Min window size: " << minW << "x" << minH << " (current " << width << "x"
                  << height << ")" << std::endl;

        // The hint only constrains what the user drags next - it never grows a
        // window that is already below it (a restored session, a smaller screen
        // last run), so the floor is enforced once here too.
        // Resize only: the cached bound has no position yet on the first pass
        // (the window is not mapped), so moving it would drop it at 0,0
        if (width < minW || height < minH) {
            m_main->window().resize(std::max(width, minW), std::max(height, minH));
        }
    }

    /**
     * @brief Apply resource changes to all windows and renderers
     */
    void apply(Ui::Res::Type::changes_t changed)
    {
        // Only what it is made of - its text, button, colours and size - closes it;
        // the next event reopens it
        const Ui::Res::Type::changes_t tooltipParts { Ui::Res::Type::Changed::Locale,
                                                      Ui::Res::Type::Changed::Button,
                                                      Ui::Res::Type::Changed::Theme,
                                                      Ui::Res::Type::Changed::Layout };
        if (changed.hasAny(tooltipParts)) {
            m_tooltipPopup.destroy();
        }

        // Layout reload may have added, removed, or renamed dock configs.
        // Wipe-and-rebuild matches initialize() exactly; persisted widths
        // survive because Ui::Render::DockColumn's ctor pulls them from ResManager's
        // by-name dock-state map. Drag capture is cleared so a stale id
        // (dock removed mid-drag) can't route into a missing entry.
        //
        // Host-projected rows are carried across by name for the same reason
        // widths are: the rebuild is our own bookkeeping, and a host must not
        // have to re-push content every time something happens to touch the
        // layout. A theme switch lands here (layout.json reads theme CSS vars,
        // so it compares unequal) via apply(), which never runs the reload
        // hooks - so "re-project in afterReload" would not even cover it.
        if (changed.has(Ui::Res::Type::Changed::Layout)) {
            m_dockDrag.cancel();
            m_docks.remake();
            // Recompute toolbar+dock margins, place each dock, push the new
            // viewport to content surfaces, and queue a main-window repaint.
            reflowDocksAndViewport();
        }

        // Window icons live in icon-defaults.json; re-read them when that file changed.
        if (changed.has(Ui::Res::Type::Changed::Icon)) {
            applyWindowIcon();
        }

        // Unconditional: the floor is derived from the layout AND the menus'
        // dialog sizes, so no single Changed bit covers it
        applyMinWindowSize();

        // Main window: setBackground() calls makeCurrent() internally
        if (m_main && changed.has(Ui::Res::Type::Changed::Theme)) {
            m_main->window().setBackground(m_resManager.theme().second.bg);
        }

        if (hasUiRenderer()) {
            m_main->pairing().apply(changed); // makes the main context current, then applies
            m_main->window().requestRender();
        }

        // Every open list and dialog, so one left open lands on a new theme with the main window
        const auto applyPopup = [changed](const auto & slot) {
            if (slot.connector) {
                slot.connector->pairing().apply(changed);
            }
        };
        m_slots.forEach(applyPopup);

        m_hostSurfaces.apply(changed, m_resManager.theme().workspace.bg);

        // Restore main window context (a content setBackground may have switched it)
        if (m_main) {
            m_main->window().makeCurrent();
        }
    }

    // The toolbar tooltip syncTooltip held back, once its delay has run out. Asked
    // every loop iteration (Shell::run): a pointer resting on a button sends nothing.
    // A menu or dialog opened since drops it, as syncTooltip would
    void showDueTooltip() { m_tooltipPopup.showDue(isChromeOpen()); }

    // A menu or dialog closed, having taken the pointer while open (Shell::openDialog
    // even clears the main window's memory of it): its last place goes back through the
    // main route, so hover, cursor and tooltip catch up without a move
    void rehover()
    {
        if (m_pointerX == Ui::NO_POINTER || m_main == nullptr || isChromeOpen()) {
            return;
        }
        if (onMouseMove(m_pointerX, m_pointerY)) {
            requestMainRender();
        }
        syncTooltip();
    }

    // Whether the pointer, whichever window has it, stands in a rect of the main window, CSS px
    [[nodiscard]] bool isPointerIn(const Ui::Res::Type::bound_t & cssBound) const
    {
        return m_pointerX != Ui::NO_POINTER && cssBound.contains(toCss(m_pointerX), toCss(m_pointerY));
    }

    /**
     * @brief Get the main window
     */
    [[nodiscard]] NativeWindow & mainWindow() { return m_main->window(); }

    // Clear main-UI hover state only (popups and docks handle their own leave).
    bool mainWindowMouseLeave() { return m_main != nullptr && m_main->pairing().onMouseLeave(); }

    /**
     * @brief Get main window's absolute screen position
     */
    void mainWindowScreenPosition(int & screenX, int & screenY) const
    {
        if (m_main) {
            m_main->window().screenPosition(screenX, screenY);
        } else {
            screenX = 0;
            screenY = 0;
        }
    }

    // Frame top-left for session persistence. Differs from
    // mainWindowScreenPosition() in that it accounts for the WM-drawn frame
    // (title bar etc) so the value round-trips through moveResize without
    // drifting on restart. See Ui::IWindow::screenFramePosition for the why.
    void mainWindowScreenFramePosition(int & screenX, int & screenY) const
    {
        if (m_main) {
            m_main->window().screenFramePosition(screenX, screenY);
        } else {
            screenX = 0;
            screenY = 0;
        }
    }

    [[nodiscard]] fpx_t windowWidth() const { return m_geometry.width(); }
    [[nodiscard]] fpx_t windowHeight() const { return m_geometry.height(); }

    // -------- Ui::Render::UiRenderer management --------

    /**
     * @brief Create Ui::Render::UiRenderer, initialize it, and transfer ownership to mainWindow
     */
    void initUiRenderer()
    {
        // The fw renderer never holds a window; give it a makeCurrent callback
        // over the main window plus the current physical size.
        m_main->emplaceRenderer(
            std::make_unique<Ui::Gl::GlRender>([&window = m_main->window()] { window.makeCurrent(); },
                                               m_resManager.resPath().fontDir()),
            m_geometry.width(),
            m_geometry.height(),
            m_resManager);
        m_main->renderer().setContent();
        m_main->renderer().setExtraOpsHook([this](Ui::Render::UiRenderer & out) { m_docks.render(out); });
        m_main->renderer().setExtraElementsHook(
            [this](Ui::Render::UiLayout & layout) { m_docks.appendElements(layout); });
    }

    /**
     * @brief Set hover callback on Ui::Render::UiRenderer
     */
    void setOnElementHover(std::function<void(Ui::Render::UiElementType, id_t)> cb)
    {
        if (hasUiRenderer()) {
            m_main->renderer().setOnElementHover(std::move(cb));
        }
    }

    /**
     * @brief Get element bound from Ui::Render::UiRenderer by element id
     */
    [[nodiscard]] const Ui::Res::Type::bound_t & elementBound(id_t id) const
    {
        static const Ui::Res::Type::bound_t empty {};
        return hasUiRenderer() ? m_main->renderer().bound(id) : empty;
    }

    /**
     * @brief Rebuild UI content (after data changes)
     */
    void setUIContent()
    {
        if (hasUiRenderer()) {
            m_main->renderer().setContent();
        }
    }

    /**
     * @brief Resize the main window Ui::Render::UiRenderer
     */
    void resizeMainRenderer(fpx_t w, fpx_t h)
    {
        if (hasUiRenderer()) {
            m_main->renderer().resize(w, h);
        }
    }

    /**
     * @brief Full render pipeline: child windows, content refresh, render queue drain.
     *
     * Call this once per frame from the main loop.
     */
    void renderAll()
    {
        // Only log when the queue carries more than the steady-state main
        // window - i.e. a child window or popup also needs a frame this tick.
        // The pending=1 case is the per-tick noise we want to suppress.
        if constexpr (WM_ROUTE_DEBUG) {
            if (const size_t pending = m_renderQueue.pending().size(); pending != 1) {
                std::cout << "[WM] renderAll pending=" << pending << std::endl;
            }
        }

        // 1. Render child windows (content surface) -- may queue main window render
        if (renderContentSurfaces()) {
            requestMainRender();
        }

        // 2. Refresh content if dirty (before draining render queue)
        //    Must restore main window GL context - child window rendering (step 1)
        //    may have left the content surface's context current, and font/texture creation here
        //    needs the main window's context.
        if (m_contentIsDirty) {
            m_main->window().makeCurrent();
            setUIContent();
            m_contentIsDirty = false;
        }

        // 3. Drain render queue (main + popup)
        m_mainFrame.renderQueued();

        // 4. A content surface can change its tooltip while it draws - a camera move
        //    leaves one behind - so it is settled per frame as well as per event
        syncTooltip();
    }

    /**
     * @brief Classify which popup-type window a (non-dialog) event belongs to.
     *
     * Matches the popup/submenu first (same type, submenu topmost), then falls
     * through to main/content-surface routing. Sets event.isPopupEvent,
     * event.childWindowId, and adjusts mouse coords.
     * @return Target popup connector, or nullptr if event is for main/child window.
     */
    MenuConnector * classifyPopupEvent(Event & event)
    {
        if (auto * hit = matchPopupTarget<MenuConnector>(
                { m_slots.submenu.connector.get(), m_slots.menu.connector.get() },
                event);
            hit != nullptr) {
            return hit;
        }

        // Composite mode routes popups by bounds (above) and content surfaces by
        // hit-test elsewhere - no handle-based classification here.
        if (g_config.isCompositing) {
            return nullptr;
        }

        // Non-composite mode: classify remaining source handles.
        const auto src = event.sourceWindow;

        if (src == 0 || (m_main && src == m_main->window().nativeHandle())) {
            if constexpr (WM_ROUTE_DEBUG) {
                if (isMouseEvent(event)) {
                    std::cout << "[WM] route=main src=" << src
                              << " main=" << (m_main ? m_main->window().nativeHandle() : NativeWindowHandle {})
                              << " type=" << static_cast<int>(event.type) << std::endl;
                }
            }
            return nullptr;
        }

        // Check content surfaces (host viewports)
        if (const id_t id = m_hostSurfaces.idOf(src); id != Ui::INVALID_ID) {
            event.childWindowId = id;
            if constexpr (WM_ROUTE_DEBUG) {
                if (isMouseEvent(event)) {
                    std::cout << "[WM] route=content src=" << src << " childId=" << id
                              << " type=" << static_cast<int>(event.type) << std::endl;
                }
            }
            return nullptr;
        }

        if constexpr (WM_ROUTE_DEBUG) {
            if (isMouseEvent(event)) {
                std::cout << "[WM] route=unknown src=" << src << " type=" << static_cast<int>(event.type) << std::endl;
            }
        }
        return nullptr;
    }

    /**
     * @brief Route a popup event to the target connector (popup/submenu/dialog).
     *
     * Events go through the connector's pairing (make current -> renderer, request render);
     * geometry (containsPoint) and the render request are on its window.
     */
    template <typename C>
    void routePopupEvent(const Event & event, C & target)
    {
        Pairing & pairing = target.pairing();
        switch (event.type) {
        case EventType::MouseMove: {
            const bool insideVisual = target.window().containsPoint(event.mouse.x, event.mouse.y);
            if (insideVisual) {
                if (pairing.onMouseMove(event.mouse.x, event.mouse.y)) {
                    target.window().requestRender();
                }
                m_popupEventConsumed = true;
                if (m_main && m_main->pairing().onMouseLeave()) {
                    requestMainRender();
                }
                // Composited, the surface gets no leave of its own
                m_hostSurfaces.leave();
            } else if (pairing.onMouseLeave()) {
                target.window().requestRender();
            }
            break;
        }
        case EventType::MouseButtonPress:
            if (event.mouse.button == MouseButton::Left) {
                const bool insideVisual = target.window().containsPoint(event.mouse.x, event.mouse.y);
                if (insideVisual) {
                    pairing.onMousePress(event.mouse.x,
                                         event.mouse.y,
                                         event.mouse.button,
                                         event.mouse.clickCount,
                                         event.mouse.modifiers);
                    target.window().requestRender();
                    m_popupEventConsumed = true;
                }
            }
            break;
        case EventType::MouseButtonRelease:
            if (event.mouse.button == MouseButton::Left) {
                const bool insideVisual = target.window().containsPoint(event.mouse.x, event.mouse.y);
                if (insideVisual) {
                    m_lastClickResult = pairing.onMouseRelease(event.mouse.x, event.mouse.y, event.mouse.button);
                    target.window().requestRender();
                    m_popupEventConsumed = true;
                }
            }
            break;
        case EventType::Scroll: {
            const bool insideVisual = target.window().containsPoint(event.mouse.x, event.mouse.y);
            if (insideVisual) {
                if (pairing.onScroll(event.mouse.x, event.mouse.y, event.scroll.deltaY).changed) {
                    target.window().requestRender();
                }
                m_popupEventConsumed = true;
            }
            break;
        }
        case EventType::MouseLeave:
            pairing.onMouseLeave();
            target.window().requestRender();
            break;
        default: break;
        }
    }

    /**
     * @brief Check if Ui::Render::UiRenderer is initialized
     */
    [[nodiscard]] bool hasUiRenderer() const { return m_main != nullptr && m_main->hasRenderer(); }

    // Whether the main window drew its last frame mid-glide, so another is due
    [[nodiscard]] bool isMainGliding() const { return hasUiRenderer() && m_main->renderer().isGliding(); }

    [[nodiscard]] bool isRunning() const { return m_isRunning.load() && !m_isStopRequested.load(); }
    void               stop()
    {
        m_isRunning       = false;
        m_isStopRequested = true;
    }

    /**
     * @brief Poll for platform events (non-blocking)
     * @return true if an event was retrieved, false if no events pending
     */
    bool pollEvent(Event & event)
    {
        if (!m_eventHandler) {
            return false;
        }
        return m_eventHandler->pollEvent(event, m_main->window());
    }

    /**
     * @brief Check if there are pending events
     */
    [[nodiscard]] bool hasPendingEvents() const
    {
        if (!m_eventHandler) {
            return false;
        }
        return m_eventHandler->hasPendingEvents(m_main->window());
    }

    /**
     * @brief Flush pending output to display server
     */
    void flush()
    {
        if (m_eventHandler) {
            m_eventHandler->flush(m_main->window());
        }
    }

    bool copyToClipboard(const std::string & text)
    {
        if (m_eventHandler && m_main) {
            return m_eventHandler->copyToClipboard(m_main->window(), text);
        }
        return false;
    }

    // While true, a dialog another program shows for the main window - the desktop's file
    // dialog - is handed the focus a desktop holds back from it
    void setDialogAwaited(bool isAwaited)
    {
        if (m_eventHandler && m_main) {
            m_eventHandler->setDialogAwaited(m_main->window(), isAwaited);
        }
    }

    /**
     * @brief Resize all child windows when main window resizes
     *
     * Called in response to OS ConfigureNotify - the main window has already
     * been resized by the OS, so we only sync internal state (no XResizeWindow).
     */
    void onMainWindowResize(fpx_t width, fpx_t height)
    {
        if constexpr (WM_ROUTE_DEBUG) {
            std::cout << "[WindowManager] onMainWindowResize: " << width << "x" << height << " (was "
                      << m_geometry.width() << "x" << m_geometry.height() << ")" << std::endl;
        }

        m_geometry.setSize(width, height);

        // Shrinking the window can leave persisted dock widths summing to
        // more than the new viewport can fit, which would force the content surface
        // viewport to clamp at 1px (or invert) below. Clamp each dock against
        // the now-smaller window first; clampStateWidth writes session.json
        // through ResManager::setDockState when a width actually shrinks, so
        // the persisted value tracks what the user can see. Refresh the
        // dock-driven side margins against the clamped widths so the
        // content surface math below uses the post-clamp totals.
        m_docks.clampToWindow();
        m_geometry.resetSideMargins(m_resManager.layout());
        applyDockMargins();

        // Re-position docks now that the window has new dimensions. Dock widths
        // didn't change, only the right-edge cursor (width less the right margin)
        // and the body height did - layOut() reads both.
        m_docks.layOut();

        // Sync main window internal dimensions first - recenter/resize reads
        // bound(). Guarded like every other main-window use here: a resize can
        // arrive before the window exists (and does, in headless tests).
        if (m_main) {
            m_main->window().syncDimensions(width, height);
        }

        // Resize UI renderer to new dimensions
        if (hasUiRenderer()) {
            m_main->renderer().resize(width, height);
        }

        m_dialog.recenter();

        m_hostSurfaces.fitToResizedWindow();
    }

    // -------- Ui::IEventApp Interface --------

    bool onMouseMove(int x, int y) override
    {
        m_tooltipPopup.followPointer(toCss(x), toCss(y));

        // Mid-drag: route exclusively to whoever holds the pointer until
        // mouse-up, so the cursor can wander into the content surface without
        // breaking the gesture.
        if (m_dockDrag.isHolding()) {
            if (const DockAction action = m_dockDrag.move(toCss(x), toCss(y)); action != DockAction::Pass) {
                follow(action);
                return true;
            }
        }
        if (m_tabDrag.move(toCss(x))) {
            return true;
        }

        // Hover update on all docks (cheap, none capture by default).
        bool dockHoverChanged = false;
        bool overGrip         = false;
        m_docks.forEach([&dockHoverChanged, &overGrip, x, y](id_t /*id*/, Ui::Render::DockColumn & dock) {
            dockHoverChanged |= dock.onMouseMove(toCss(x), toCss(y));
            overGrip |= dock.isHoveredGrip();
        });
        if (dockHoverChanged) {
            m_renderQueue.request(MAIN_WINDOW_ID);
        }

        // The grip is a resize handle - say so before the drag rather than after; over
        // a content surface, whatever its host asked for (setContentPointer)
        const std::optional<content_hit_t> hit = m_hostSurfaces.hover(x, y);
        if (overGrip) {
            setCursor(Ui::Window::PointerShape::ResizeH);
        } else {
            setCursor(hit.has_value() ? m_hostSurfaces.activePointer() : Ui::Window::PointerShape::Default);
        }

        if (hit.has_value()) {
            if (m_main) {
                m_main->pairing().onMouseLeave();
            }
            return hit->pairing.get().onMouseMove(hit->lx, hit->ly);
        }

        // Cursor left the content viewport but stayed in the app; clear the
        // active surface's drag state so it doesn't resume on re-entry.
        if (const auto active = activeContentPairing()) {
            active->get().onMouseLeave();
        }

        m_menus.leave();

        return m_main ? m_main->pairing().onMouseMove(x, y) : dockHoverChanged;
    }

    // What a dock gesture asks for done (DockDrag): its rows' hit rects built again, or the
    // docks and the viewport laid out again - margins, dock positions and content surface
    // size in one pass, so the content child shrinks/grows live as the user drags
    void follow(DockAction action)
    {
        if (action == DockAction::Refresh) {
            requestContentRefresh();
        } else if (action == DockAction::Reflow) {
            reflowDocksAndViewport();
        }
    }

    Ui::Render::element_event_t onMousePress(int                         x,
                                             int                         y,
                                             Ui::Window::MouseButton     button,
                                             int                         clickCount,
                                             Ui::Window::key_modifiers_t modifiers) override
    {
        // A press on anything that declared DragStart takes the pointer before
        // content-surface routing gets a look: grip rects sit flush against
        // (and can round into) the content edge. Left only - the other buttons
        // belong to whatever the press lands on.
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);
        if (button == Ui::Window::MouseButton::Left && m_main) {
            const Ui::Render::UiElement * grabbed = m_main->renderer().hitTest(cssX,
                                                                               cssY,
                                                                               Ui::Render::EventKind::DragStart);
            if (grabbed != nullptr) {
                if (const DockAction action = m_dockDrag.begin(*grabbed, cssX, cssY, clickCount);
                    action != DockAction::Pass) {
                    follow(action);
                    m_renderQueue.request(MAIN_WINDOW_ID);
                    return { .changed = true };
                }
            }
            // A tab held - its close glyph is no tab - and the press goes on as one, so a
            // release short of the click threshold is still its click (TabDrag)
            const Ui::Render::UiElement * pressed = m_main->renderer().hitTest(cssX,
                                                                               cssY,
                                                                               Ui::Render::EventKind::LeftClick);
            if (pressed != nullptr && pressed->type == Ui::Render::UiElementType::Tab) {
                m_tabDrag.press(*pressed, cssX);
            }
        }

        if (const auto hit = hitTestActiveContent(x, y)) {
            return hit->pairing.get().onMousePress(hit->lx, hit->ly, button, clickCount, modifiers);
        }
        return m_main ? m_main->pairing().onMousePress(x, y, button, clickCount, modifiers)
                      : Ui::Render::element_event_t {};
    }

    Ui::Render::element_event_t onMouseRelease(int x, int y, Ui::Window::MouseButton button) override
    {
        // Mid-drag release: commit state and reflow content surface. A drag can
        // only have started on Left, so only Left ends it
        if (m_dockDrag.isHolding() && button == Ui::Window::MouseButton::Left) {
            follow(m_dockDrag.end(toCss(x)));
            return {};
        }

        // A held tab let go: a drag ends there, no click, and the hover catches up; short of
        // the threshold it was a click, and goes on as one
        if (button == Ui::Window::MouseButton::Left && m_tabDrag.drop()) {
            rehover();
            return {};
        }

        if (const auto hit = hitTestActiveContent(x, y)) {
            return hit->pairing.get().onMouseRelease(hit->lx, hit->ly, button);
        }
        return m_main ? m_main->pairing().onMouseRelease(x, y, button) : Ui::Render::element_event_t {};
    }

    bool onMouseLeave() override
    {
        bool changed = false;
        m_hostSurfaces.clearPointer();

        // Active content surface can be mid-drag; clear its drag state too
        if (const auto active = activeContentPairing()) {
            changed |= active->get().onMouseLeave();
        }
        if (m_main) {
            changed = m_main->pairing().onMouseLeave();
        }
        m_slots.forEach([&changed](const auto & slot) {
            if (slot.connector) {
                changed |= slot.connector->pairing().onMouseLeave();
            }
        });

        // Docks: clear hover and finalize any in-flight drag. Ui::Render::DockColumn::onMouseLeave
        // commits the transient width when the drag was meaningful (>= clickThreshold)
        // so a drag-to-zero followed by release outside the window collapses the dock
        // as the user expects; small/no-drag leaves fall through as a no-op.
        // Cursor wandering into the content surface mid-drag doesn't reach here - that's
        // handled by the capture in onMouseMove.
        const bool wasDragging = m_dockDrag.isHolding();
        m_docks.forEach([](id_t /*id*/, Ui::Render::DockColumn & dock) { dock.onMouseLeave(); });
        if (wasDragging) {
            m_dockDrag.cancel();
            // Width may have changed (either committed-on-leave or, below threshold,
            // reverted to start width). Reflow so dock layout + content surface pick up
            // the final state instead of holding the last drag-step size.
            reflowDocksAndViewport();
            changed = true;
        }
        // A tab drag too, as its release may never come (a dialog opening, a capture lost)
        changed |= m_tabDrag.drop();

        return changed;
    }

    Ui::Render::element_event_t onScroll(int x, int y, fpx_t deltaY) override
    {
        // A dock owns the wheel over its own column, ahead of the content surface
        // - the same precedence the DragStart hit test takes on press. The caller
        // discards this result, so the repaint is requested here
        const id_t underId = m_docks.idWhere(
            [x, y](const Ui::Render::DockColumn & dock) { return dock.outer().contains(toCss(x), toCss(y)); });
        bool isScrolled = false;
        if (m_docks.visit(underId, [&isScrolled, deltaY](Ui::Render::DockColumn & dock) {
                isScrolled = dock.onScroll(deltaY);
            })) {
            if (!isScrolled) {
                return {};
            }
            requestContentRefresh();
            return { .event = Ui::Render::EventKind::Scroll, .changed = true };
        }

        // Scroll passes raw (main-local) coords, matching the prior contract -
        // the renderer uses scroll for zoom, not a hit point.
        if (const auto hit = hitTestActiveContent(x, y)) {
            return hit->pairing.get().onScroll(x, y, deltaY);
        }
        return m_main ? m_main->pairing().onScroll(x, y, deltaY) : Ui::Render::element_event_t {};
    }

    // Child-routed overloads (child-local coordinates, bypass hit-testing).
    // childId is a content-surface id; coords are already in its frame.

    // A surface's own events never pass the main window's hit test, so its pointer is
    // set here - a child window with none of its own shows its parent's
    bool onMouseMove(int x, int y, id_t childId)
    {
        const auto entered = m_hostSurfaces.enter(childId);
        if (!entered.has_value()) {
            return onMouseMove(x, y);
        }
        m_tooltipPopup.leaveButtons(); // on a content surface, so off every button
        // Off the main window's chrome too: X tells it no leave into a child of its own, so its
        // hover goes here, the main window's alone - an app-wide leave would end a drag in the surface
        if (m_main && m_main->pairing().onMouseLeave()) {
            requestMainRender();
        }
        return entered->get().onMouseMove(x, y);
    }

    Ui::Render::element_event_t onMousePress(int                         x,
                                             int                         y,
                                             id_t                        childId,
                                             Ui::Window::MouseButton     button,
                                             int                         clickCount,
                                             Ui::Window::key_modifiers_t modifiers)
    {
        if (const auto content = contentPairing(childId)) {
            return content->get().onMousePress(x, y, button, clickCount, modifiers);
        }
        return onMousePress(x, y, button, clickCount, modifiers);
    }

    Ui::Render::element_event_t onMouseRelease(int x, int y, id_t childId, Ui::Window::MouseButton button)
    {
        if (const auto content = contentPairing(childId)) {
            return content->get().onMouseRelease(x, y, button);
        }
        return onMouseRelease(x, y, button);
    }

    Ui::Render::element_event_t onScroll(int x, int y, fpx_t deltaY, id_t childId)
    {
        if (const auto content = contentPairing(childId)) {
            return content->get().onScroll(x, y, deltaY);
        }
        return onScroll(x, y, deltaY);
    }

    /**
     * @brief Shutdown all child windows and cleanup
     */
    void shutdown()
    {
        if (!m_main) {
            return;
        }

        std::cout << "[WindowManager] Shutting down..." << std::endl;

        m_isRunning = false;

        // Every popup-type window closed, the tooltip forgetting what it showed too
        m_tooltipPopup.destroy();
        m_slots.forEach([this](auto & slot) { m_popups.close(slot); });

        // Clean up GL resources in main window's context
        m_main->window().makeCurrent();
        m_compositor.cleanup();
        m_slots.forEach([](auto & slot) { slot.composite.destroy(); });
        m_hostSurfaces.clear();

        // Drop docks before ResManager nulls below; they hold const references
        // into ResManager's layout.
        m_docks.clear();

        // Destroy event handler before main window - pointer/keyboard proxies
        // require a live display connection
        m_eventHandler.reset();

        // Connector dtor destroys the renderer first (window context still
        // alive), then the window's EGL/display
        m_main.reset();

        std::cout << "[WindowManager] Shutdown complete" << std::endl;
    }

    /**
     * @brief Render the active content surface if queued (HostSurfaces::render).
     *
     * Call from the main loop. Returns true when the main window must re-render.
     */
    bool renderContentSurfaces() { return m_hostSurfaces.render(isChromeOpen() || hasTooltip()); }
};

} // namespace Ui::Window
