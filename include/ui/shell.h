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
#include "common/time.h"
#include "sig.h"
#include "ui/action/actionmap.h"
#include "ui/action/openpickedfiles.h"
#include "ui/action/opentabswitcher.h"
#include "ui/action/registry.h"
#include "ui/config.h"
#include "ui/gl/svgdocuments.h"
#include "ui/idkind.h"
#include "ui/inithooks.h"
#include "ui/intent.h"
#include "ui/interface/ichromecommands.h"
#include "ui/io/filedialog.h"
#include "ui/menubar.h"
#include "ui/render/context.h"
#include "ui/render/elementevent.h"
#include "ui/render/renderscope.h"
#include "ui/render/tabsshownof.h"
#include "ui/render/tabstripwidthof.h"
#include "ui/render/uielement.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/dialogaction.h"
#include "ui/res/type/layout.h"
#include "ui/res/type/menu.h"
#include "ui/resolvedcontentof.h"
#include "ui/result.h"
#include "ui/shelldebug.h"
#include "ui/statusline.h"
#include "ui/tabbar.h"
#include "ui/tabswitch.h"
#include "ui/tabswitcher.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"
#include "ui/window/iscontrolkey.h"
#include "ui/window/keymodifier.h"
#include "ui/window/windowmanager.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstddef>
#include <exception>
#include <functional>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui {

/**
 * @brief The runnable framework core - owns the fw object graph.
 *
 * Domain-blind: composes ResManager + WindowManager + Context + Action::Registry
 * (the chrome, windowing, intent mapping, and action dispatch table). It owns
 * the run loop, init spine, default shell actions, the event->intent dispatch,
 * and all interactive-chrome orchestration, through its parts: the open popup menu
 * (MenuBar), the status text (StatusLine), the Ctrl+Tab list (TabSwitcher), and
 * the dialog close itself. A host composes a Shell, adds content +
 * domain actions, and wires the domain hooks (tab activate/close, key press).
 */
class Shell final : public Ui::IChromeCommands, private Common::NonCopyable {
public:
    // No banner here: m_resManager is constructed - and logs its app.json load -
    // before this body runs, so anything printed here can never be the first line.
    // main() owns the greeting.
    Shell()
    {
        // Bind the built-in actions here, not in wireEvents(), so the registry is
        // complete before a host registers anything. A host overriding a key
        // (e.g. Reload) just calls actions().on() afterwards and wins by
        // overwrite - there is no registration-order rule to remember.
        Ui::Action::registerActions(*this);
    }

    ~Shell() override = default;

    [[nodiscard]] Ui::Res::ResManager &       resManager() { return m_resManager; }
    [[nodiscard]] const Ui::Res::ResManager & resManager() const { return m_resManager; }

    [[nodiscard]] Ui::Window::WindowManager &       windowManager() { return m_windowManager; }
    [[nodiscard]] const Ui::Window::WindowManager & windowManager() const { return m_windowManager; }

    [[nodiscard]] Ui::Io::FileDialog & fileDialog() { return m_fileDialog; }

    [[nodiscard]] Ui::Render::Context &       context() { return m_context; }
    [[nodiscard]] const Ui::Render::Context & context() const { return m_context; }

    [[nodiscard]] Ui::Action::Registry &       actions() { return m_actions; }
    [[nodiscard]] const Ui::Action::Registry & actions() const { return m_actions; }

    // Host hooks (domain-blind seams; unset on a standalone shell):
    // - frameTasks runs once at the top of every loop iteration (host async drain).
    // - onTick runs after an event batch is processed (host per-frame upkeep).
    void setFrameTasks(Ui::task_fn_t fn) { m_frameTasks = std::move(fn); }
    void setOnTick(Ui::task_fn_t fn) { m_onTick = std::move(fn); }

    // Domain hooks: the chrome fires these on tab activate/close + key press so
    // the host runs the domain side (workspaces, content surfaces, mouse-rotation toggle).
    void setOnTabActivated(std::function<void(id_t)> fn) { m_onTabActivated = std::move(fn); }
    void setOnTabClosed(std::function<void(id_t)> fn) { m_onTabClosed = std::move(fn); }

    // -- IChromeCommands: the window/chrome commands routeIntent() drives. The
    // domain-coupled ones (tabs, dock rows) fire host hooks

    // Activate a tab as a click on it does. An action that picks the tab itself
    // (CycleTab) calls it too, so the strip scrolls it into view - a click's tab is
    // already in it. A click on a row of the Ctrl+Tab list is one on its tab, and the
    // list goes down with any switch
    void setActiveTab(id_t tabId) override
    {
        m_tabSwitcher.cancel();
        if (m_onTabActivated) {
            m_onTabActivated(tabId);
        }
        Ui::TabBar & tabBar = m_resManager.tabBar();
        const auto   order  = tabBar.order();
        const auto   it     = std::ranges::find(order, tabId);
        if (it == order.end()) {
            return;
        }
        const std::size_t before = tabBar.scrollOffset();
        tabBar.reveal(static_cast<std::size_t>(std::ranges::distance(order.begin(), it)), tabsShown());
        if (tabBar.scrollOffset() != before) {
            m_windowManager.requestContentRefresh();
        }
    }

    // The Ctrl+Tab list while Ctrl is held, up or not yet: what OpenTabSwitcher opens, and the
    // shell's key, release and pointer handling drive
    [[nodiscard]] Ui::TabSwitch & tabSwitch() { return m_tabSwitcher.tabSwitch(); }

    // The Ctrl+Tab list's highlight `delta` rows on, and its window showing it
    void cycleTabSwitcher(int delta) { m_tabSwitcher.cycle(delta); }

    void emitAction(const std::string & actionKey, const std::string & arg) override
    {
        // arg is the item label (value) for parameterized actions, "" otherwise.
        m_actions.dispatch(actionKey, arg);
    }

    [[nodiscard]] bool isPopupOpen() const override { return m_menuBar.isOpen(); }

    void openPopup(id_t menuId) override
    {
        m_menuBar.trace("opened", menuId);
        m_menuBar.open(menuId);
    }

    // routeIntent has already gated this on isPopupOpen()
    void closePopup() override { m_menuBar.closeAfterEvent(); }

    void openDialog(id_t itemId) override
    {
        const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(itemId);
        m_menuBar.forgetHover();
        m_windowManager.onMouseLeave();
        m_windowManager.subscribe().defer([this, dialog = item.dialog]() { m_windowManager.dialog().open(dialog); });
    }

    // A tab's close button - the strip's, or a row's in the Ctrl+Tab list, which then stays up,
    // Ctrl still held, with the tabs the host has left
    void closeTab(id_t tabId) override
    {
        if (m_onTabClosed) {
            m_onTabClosed(tabId);
        }
        m_tabSwitcher.refresh();
    }

    void activateRow(id_t rowId) override
    {
        if (m_onDockRowActivated) {
            m_onDockRowActivated(rowId);
        }
    }

    void openRowMenu(id_t elementId) override { m_menuBar.openRowMenu(elementId); }

    void toggleRow(id_t rowId) override
    {
        if (m_onDockRowToggled) {
            m_onDockRowToggled(rowId);
        }
    }

    void copyText(const std::string & text) override
    {
        if (m_windowManager.copyToClipboard(text)) {
            showTempStatus(m_resManager.localeManager().get("StatusCopiedToClipboard"));
        }
    }

    // Dock rows the host projected via WindowManager::setDockRows. rowId is the
    // host's own row id, so it must be unique across docks - the framework
    // carries it opaquely and cannot tell two docks' rows apart.
    void setOnDockRowActivated(std::function<void(id_t)> fn) { m_onDockRowActivated = std::move(fn); }

    // A slider row being dragged: (row id, 0..1). Forwarded straight to
    // WindowManager, which owns the drag, rather than kept here - there is no
    // shell-level decision to make on the way through.
    void setOnDockRowValue(std::function<void(id_t, fpx_t)> fn) { m_windowManager.setOnRowValue(std::move(fn)); }
    // A tab dragged along the strip moved: the tab, and the one it now stands right before
    // (INVALID_ID the end). The host moves it in its order and projects the tabs back
    void setOnTabMoved(std::function<void(id_t, id_t)> fn) { m_windowManager.setOnTabMoved(std::move(fn)); }
    void setOnDockRowToggled(std::function<void(id_t)> fn) { m_onDockRowToggled = std::move(fn); }
    void setOnKeyPress(Ui::predicate_fn_t fn) { m_onKeyPress = std::move(fn); }

    // Register a loader for a file extension (lowercase, no dot, e.g. "step").
    // OpenFile routes picked files here; an extension with no handler shows a
    // warning dialog. The standalone shell registers none, so any open warns.
    void setFileHandler(const std::string & extension, Ui::action_fn_t handler)
    {
        m_fileHandlers[extension] = std::move(handler);
    }

    // Run the handler registered for a file's extension; returns false (no-op)
    // when none is registered, so the caller can show the no-handler warning.
    bool runFileHandler(const std::string & extension, const std::string & file) const
    {
        const auto handler = m_fileHandlers.find(extension);
        if (handler == m_fileHandlers.end()) {
            return false;
        }
        handler->second(file);
        return true;
    }

    void requestStop() { m_windowManager.stop(); }
    void shutdown() { m_windowManager.shutdown(); }

    // The application lifecycle in one call: signal handlers, session restore,
    // init, run, session save, and an exit code. Optional convenience - a host
    // that needs its own process policy (an embedder with its own signal handling
    // or exit conventions) drives initialize()/run()/shutdown() directly instead.
    //
    // Deliberately does NOT shut down: teardown belongs to whoever owns this Shell,
    // because a host with content surfaces must destroy those FIRST (child windows
    // share the main window's display). ~WindowManager covers the standalone case.
    //
    // No Hello!/Bye! banner either, for the same reason: this is a member function,
    // so our own members (ResManager logging its app.json load) are already
    // constructed before it can print, and it returns before we are destroyed. Only
    // main() brackets the whole lifetime, so main() owns the banner.
    [[nodiscard]] int runApp(const Ui::init_hooks_t & hooks = {})
    {
        int exitCode = 1;
        try {
            g_app_init([this]() { requestStop(); });
            (void)std::signal(SIGINT, on_signal);
            (void)std::signal(SIGTERM, on_signal);

            m_resManager.loadSession(m_resManager.sessionPath());

            if (initialize(hooks)) {
                run();
                // A host that writes the session off-thread finishes that write here:
                // landing after this one, its older snapshot would overwrite the newest
                if (m_hooks.beforeExitSave) {
                    m_hooks.beforeExitSave();
                }
                m_resManager.saveSession(); // persist on clean exit (incl. SIGINT/SIGTERM)
                exitCode = 0;
            } else {
                std::cerr << "[Shell] Initialization failed" << std::endl;
            }
        } catch (const std::exception & e) {
            std::cerr << "[Shell] Error: " << e.what() << std::endl;
        }

        return exitCode;
    }

    // Reload-with-open-chrome: remember any open dialog/menu, close it, replay the
    // load cycle (the SAME one initialize() runs, hooks included), apply what
    // changed, then reopen what was open. Takes no host callback: the domain steps
    // come from the hooks stored at initialize(), which is what keeps startup and
    // reload from producing different chrome. The open menu is remembered as a
    // single hierarchical key (the deepest active node), since numeric ids are
    // reassigned by loadAll() but label-derived keys survive it.
    void reloadChrome()
    {
        const bool                    hadDialog   = m_windowManager.dialog().isOpen();
        const Ui::Res::Type::dialog_t savedDialog = hadDialog ? m_windowManager.dialog().lastData()
                                                              : Ui::Res::Type::dialog_t {};
        const Ui::key_t               activeKey   = m_menuBar.activeKey();

        // Close open popup/dialog
        if (hadDialog) {
            m_windowManager.dialog().close();
        }
        m_menuBar.close();

        loadAllResources();
        gateAndDisableUnhandled();

        m_windowManager.apply(m_resManager.changed());
        m_windowManager.requestContentRefresh();

        // Re-query display DPI: the environment may have changed since init
        // (system zoom, monitor move) and every CSS->physical conversion below
        // (popup geometry, dock margins, fonts) reads g_config.scale. On a
        // scale change, run the same-size resize cascade so the renderer and
        // content surfaces pick up the new pixel math before anything reopens.
        if (m_windowManager.refreshDisplayMetrics()) {
            m_windowManager.onMainWindowResize(m_windowManager.windowWidth(), m_windowManager.windowHeight());
            // The layout's fonts rebuilt at the new scale, or a list reopened below is measured
            // with the old ones
            m_windowManager.renderAll();
        }

        // Reopen what was open
        if (hadDialog) {
            m_windowManager.dialog().open(savedDialog);
        } else if (!activeKey.empty()) {
            m_menuBar.restore(activeKey);
        }

        if (m_hooks.afterReload) {
            m_hooks.afterReload();
        }
    }

    // Close any open popup menu (no-op if none). Generic chrome; the host calls
    // it before opening a native modal (e.g. the file dialog).
    void closePopupMenu() { m_menuBar.close(); }

    // The status bar (StatusLine): whether a temporary message shows, the real status line,
    // and a temporary message over it - the feedback an action gives on what it just did
    [[nodiscard]] bool hasTempStatus() const { return m_statusLine.hasTemp(); }
    void               setStatusText(const std::string & text) { m_statusLine.set(text); }
    void               showTempStatus(const std::string & text) { m_statusLine.showTemp(text); }

    // --- init spine: framework-internal steps. initialize() owns their order; a
    // host supplies its domain steps as init_hooks_t instead of calling these. ---

    [[nodiscard]] bool initWindow() { return m_windowManager.initialize(); }

    // Preload the SVG icons referenced by button definitions.
    void preloadButtonIcons()
    {
        size_t preloaded = 0;
        for (const auto & button : m_resManager.buttons()) {
            if (button.icon.empty()) {
                continue;
            }
            if (!Ui::Gl::SvgDocuments::loadFromFile(m_resManager.resPath().iconPathOf(button.icon)).empty()) {
                ++preloaded;
            }
        }
        std::cout << "[Shell] Preloaded " << preloaded << " SVG button icons" << std::endl;
    }

    // Create the UI renderer, size it to the window, and queue the first frame.
    void initRenderer()
    {
        m_windowManager.initUiRenderer();
        m_windowManager.resizeMainRenderer(m_windowManager.windowWidth(), m_windowManager.windowHeight());
        m_windowManager.requestContentRefresh();
    }

    // Grey out every menu item and toolbar button whose actionKey has no registered
    // handler, so the chrome never offers a click that does nothing. Both
    // initialize() and reloadChrome() run this for you (via gateAndDisableUnhandled),
    // so a host only calls it after registering an action LATE - i.e. after
    // initialize(). Dialog/submenu items stay enabled - the shell drives them
    // without an Action::Registry entry.
    void disableUnhandledItems()
    {
        m_resManager.disableUnhandledItems([this](const std::string & key) { return m_actions.has(key); });
    }

    // Run the init spine. A host supplies its domain steps as hooks rather than
    // driving the steps itself, so the framework keeps sole ownership of the
    // ordering - the first two hooks are replayed identically by every reload.
    //
    // A host must register its domain actions BEFORE calling this: the disable
    // pass below runs inside the spine, and an action registered after it returns
    // leaves its menu items greyed until the next reload.
    [[nodiscard]] bool initialize(const Ui::init_hooks_t & hooks = {})
    {
        m_hooks = hooks;

        loadAllResources();
        if (!initWindow()) {
            return false;
        }
        // The session holds the window's state from the start, as each resize keeps it: an X11 window
        // reports none, and with no window manager no resize may come to clear what a Wayland run left
        m_resManager.setSessionWindowFullscreen(m_windowManager.mainWindow().isFullscreen());
        m_windowManager.mainWindow().makeCurrent();
        if (m_hooks.afterWindowCreated) {
            m_hooks.afterWindowCreated();
        }
        gateAndDisableUnhandled();
        preloadButtonIcons();
        initRenderer();
        wireEvents();
        if (m_hooks.afterInit) {
            m_hooks.afterInit();
        }
        return true;
    }

    // Register the generic OS-event subscriptions, hover wiring, and the
    // dialog-close handler. The built-in actions are NOT bound here - the
    // constructor does that, so the registry is complete before a host adds to it.
    void wireEvents()
    {
        auto & sub = m_windowManager.subscribe();

        // CloseRequested
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::CloseRequested), Ui::APP_ID, [this]() {
            std::cout << "[Event] Window close requested" << std::endl;
            m_windowManager.stop();
        });

        // Resize
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::Resize), Ui::APP_ID, [this]() {
            const auto & event = m_windowManager.currentEvent();
            if (event.childWindowId != Ui::INVALID_ID) {
                return;
            }
            const fpx_t newW = event.resize.width;
            const fpx_t newH = event.resize.height;

            // Persist current geometry on every ConfigureNotify - covers both
            // size changes (the branch below) and move-only events (no size
            // delta, so the early-out skips handleResize but we still want
            // the new x/y in session.json). Use the frame top-left, not the
            // client area: moveResize on a managed top-level window takes
            // frame coordinates, so saving client position would drift the
            // window down by the title-bar height on every restart.
            // setSessionWindowGeometry is diff-checked and the disk write
            // is off-thread, so the worst case during a drag is one server
            // round-trip per pixel.
            int frameX = 0;
            int frameY = 0;
            m_windowManager.mainWindowScreenFramePosition(frameX, frameY);
            // Fullscreen keeps the window it was: its geometry is what a later run makes windowed
            const bool isFullscreen = m_windowManager.mainWindow().isFullscreen();
            m_resManager.setSessionWindowFullscreen(isFullscreen);
            if (!isFullscreen) {
                m_resManager.setSessionWindowGeometry(frameX, frameY, static_cast<int>(newW), static_cast<int>(newH));
            }

            if (newW != m_windowWidth || newH != m_windowHeight) {
                if constexpr (SHELL_DEBUG) {
                    std::cout << "[Shell] Resize event: " << newW << "x" << newH << " (current: " << m_windowWidth
                              << "x" << m_windowHeight << ")" << std::endl;
                }
                m_windowWidth  = newW;
                m_windowHeight = newH;

                // Hide popup during resize (recreated in handleResize). A child
                // popup tracks the parent itself, so leave it - hiding/recreating
                // it every resize event is what makes it blink.
                m_menuBar.hideForResize();

                // Defer resize (only once per batch -- last dimensions win)
                if (!m_pendingResize) {
                    m_pendingResize = true;
                    m_windowManager.subscribe().defer([this]() { handleResize(); });
                }
            }
        });

        // ScaleChange: the main window may stand at another DPI. Laid out again by the reload
        // Ctrl+R runs, which reads the DPI afresh and replays the host's hooks - once per
        // batch, and not at all for a change that leaves the DPI as it is
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::ScaleChange), Ui::APP_ID, [this]() {
            if (m_isRescalePending || m_windowManager.askedDpi() == g_config.dpi) {
                return;
            }
            m_isRescalePending = true;
            m_windowManager.subscribe().defer([this]() {
                m_isRescalePending = false;
                reloadChrome();
            });
        });

        // FocusLost: an open list closes, as every OS's own menu does when another app takes
        // the focus, and a switch waiting to make one stops; a dialog stays for the user's return
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::FocusLost), Ui::APP_ID, [this]() {
            m_menuBar.closeForFocusLoss();
            // Ctrl going up elsewhere is never seen here, so the Ctrl+Tab list stops unswitched
            m_tabSwitcher.cancel();
        });

        // MouseButtonPress
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::MouseButtonPress), Ui::APP_ID, [this]() {
            if (m_windowManager.dialog().isOpen()) {
                return;
            }

            const auto & event = m_windowManager.currentEvent();
            m_menuBar.forgetClosingPress();
            if constexpr (MENU_HOVER_DEBUG) {
                std::cout << "[MenuHover] press: list=" << event.isPopupEvent << " at " << event.mouse.x << ","
                          << event.mouse.y << " clickCount=" << event.mouse.clickCount << std::endl;
            }

            // The Ctrl+Tab list: a press on it is its row's, which the release switches to;
            // one off it takes it down unswitched and goes on as a press
            if (m_tabSwitcher.isOpen()) {
                if (event.isPopupEvent && m_windowManager.tabSwitcher().isOpen()) {
                    return;
                }
                m_tabSwitcher.cancel();
            }

            // Popup press -- only handle outside-click policy (inside forwarding done by dispatchEvent)
            if (event.isPopupEvent && m_windowManager.menus().isOpen()) {
                m_menuBar.pressOnList(event);
                return;
            }

            // Non-popup press -- close popup if clicking outside of it
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] MouseButtonPress event received at (" << event.mouse.x << "," << event.mouse.y
                          << ") clickCount=" << event.mouse.clickCount << std::endl;
            }
            m_menuBar.pressBesideList(event);
            // A title opens, closes or switches its list on the press (isListTitle)
            if (const Ui::Render::element_event_t & press = m_windowManager.lastPressResult();
                m_menuBar.isListTitle(press)) {
                m_menuBar.trace("pressed a title", press.id);
                dispatchClick(press);
            }
            // Main window press routing already done by dispatchEvent
            if (event.childWindowId == Ui::INVALID_ID) {
                m_windowManager.requestMainRender();
            }
        });

        // MouseButtonRelease
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::MouseButtonRelease), Ui::APP_ID, [this]() {
            if (m_windowManager.dialog().isOpen()) {
                return;
            }
            // The Ctrl+Tab list's thumb let go over a row: the highlight goes there, as a move would take it
            m_tabSwitcher.followHover();

            const auto & event = m_windowManager.currentEvent();
            if constexpr (MENU_HOVER_DEBUG) {
                const auto & click = m_windowManager.lastClickResult();
                std::cout << "[MenuHover] release: list=" << event.isPopupEvent
                          << " consumed=" << m_windowManager.popupEventConsumed() << " at " << event.mouse.x << ","
                          << event.mouse.y << " click type=" << static_cast<int>(click.type) << " on '"
                          << (click.id == Ui::INVALID_ID ? std::string {} : m_resManager.findMenuItem(click.id).label)
                          << "'" << std::endl;
            }

            // Popup release -- handle click result or close
            if (event.isPopupEvent && m_windowManager.menus().isOpen()) {
                if (m_windowManager.popupEventConsumed()) {
                    const auto & result = m_windowManager.lastClickResult();
                    if (result.id != Ui::INVALID_ID) {
                        dispatchClick(result);
                    }
                } else {
                    if constexpr (SHELL_DEBUG) {
                        std::cout << "[Shell] Popup MouseButtonRelease outside visual region - closing popup"
                                  << std::endl;
                    }
                    m_menuBar.destroy();
                }
                return;
            }

            // Main/child release -- handle click result. A title's press did its work, so
            // its release does nothing
            const auto & result = m_windowManager.lastClickResult();
            if (result.id != Ui::INVALID_ID && !m_menuBar.isListTitle(result)) {
                dispatchClick(result);
            }
            if (event.childWindowId == Ui::INVALID_ID) {
                m_windowManager.requestMainRender();
            }
        });

        // MouseMove
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::MouseMove), Ui::APP_ID, [this]() {
            if (m_windowManager.dialog().isOpen()) {
                return;
            }

            const auto & event = m_windowManager.currentEvent();
            if constexpr (MENU_HOVER_DEBUG) {
                if (m_menuBar.isOpen() || m_menuBar.isSwitchPending()) {
                    std::cout << "[MenuHover] move with a list open: list=" << event.isPopupEvent
                              << " content=" << (event.childWindowId != Ui::INVALID_ID) << " at " << event.mouse.x
                              << "," << event.mouse.y << std::endl;
                }
            }

            m_tabSwitcher.followHover();

            // Popup move -- handle menu bar hover detection
            if (event.isPopupEvent) {
                m_menuBar.moveOverBarFromList(event);
                return;
            }

            // Child move -- clear ONLY main-window UI hover. The app-wide
            // onMouseLeave() would also reset the active content surface's drag
            // state, which kills rotation right after the first frame: press
            // sets dragging=true, first move rotates + notifies, this branch
            // would then clear dragging before the second move arrives.
            if (event.childWindowId != Ui::INVALID_ID) {
                if (m_windowManager.mainWindowMouseLeave()) {
                    m_windowManager.requestMainRender();
                }
                return;
            }

            // Main window move -- render queued by forwardEvent on hover change
            m_menuBar.hoverAt(event.mouse.x, event.mouse.y);
        });

        // Scroll: the Ctrl+Tab list's rows moved under the pointer, so the highlight follows the
        // row now under it, as on a move
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::Scroll), Ui::APP_ID, [this]() {
            m_tabSwitcher.followHover();
        });

        // KeyPress
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::KeyPress), Ui::APP_ID, [this]() {
            handleKeyPress(m_windowManager.currentEvent());
        });

        // KeyRelease: Ctrl going up switches to what the Ctrl+Tab list has highlighted
        sub.add(Ui::idOf(Ui::IdKind::Event, Ui::Window::EventType::KeyRelease), Ui::APP_ID, [this]() {
            if (m_tabSwitcher.isOpen() && Ui::Window::isControlKey(m_windowManager.currentEvent().key.name.data())) {
                commitTabSwitcher();
            }
        });

        // Hover handler for menu switching.
        m_windowManager.setOnElementHover([this](Ui::Render::UiElementType type, id_t id) {
            (void)type;
            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Menu button hover: id=" << id << std::endl;
            }
            m_menuBar.trace("element hover", id);
            m_menuBar.hover(id);
        });

        // Dialog content placeholders (%VERSION%/%CPU%/%RAM%/%GPU%/%GL%/%DISPLAYS%) resolved
        // from APP_VERSION + Common::System + the main window's GL context and displays.
        m_windowManager.dialog().setContentResolver([this](const std::string & content) {
            return Ui::resolvedContentOf(content, m_windowManager.mainWindow());
        });

        // Dialog close: WindowManager invokes this (deferred), not a cross-thread event.
        m_windowManager.dialog().setOnClose([this]() {
            const auto   action = m_windowManager.dialog().lastAction();
            const auto & dialog = m_windowManager.dialog().lastData();

            if (action == Ui::Res::Type::DialogAction::CopyLink && !dialog.link.empty()) {
                const std::string & linkText = m_resManager.localeManager().get(dialog.link);
                if (!linkText.empty() && m_windowManager.copyToClipboard(linkText)) {
                    showTempStatus(m_resManager.localeManager().get("StatusCopiedToClipboard"));
                }
            }

            // Close popup menu if still open
            m_menuBar.close();

            if constexpr (SHELL_DEBUG) {
                std::cout << "[Shell] Dialog closed with result: " << static_cast<int>(action) << std::endl;
            }
        });
    }

    // The main event loop: drain host frame tasks, poll+dispatch OS events,
    // run the generic per-frame upkeep + host tick, render the dirty set. Built
    // purely on WindowManager primitives, so it runs with or without content.
    void run()
    {
        // A smoke run (CI's ASan step) stops itself after its first pass, through SIGINT so the
        // signal path is tested too: no clock to guess, however slow the machine. That pass
        // draws the frame initialize() asked for, in the render below or in an event's own
        // (a resize draws at once), so its end is the first frame's, whichever drew it
        bool isStopAfterFrame = !Common::System::environmentVariable(STOP_AFTER_FRAME_VARIABLE).empty();
        while (m_windowManager.isRunning()) {
            const auto passStart = Common::Time::now();
            if (m_frameTasks) {
                m_frameTasks();
            }
            takePickedFiles();

            bool              hadEvents = false;
            Ui::Window::Event event;
            while (m_windowManager.pollEvent(event)) {
                m_windowManager.dispatchEvent(event);
                hadEvents = true;
            }

            if (hadEvents) {
                m_menuBar.followWindowMove(); // popup-position tracking during window move
                m_statusLine.expire();        // expire + restore an active temp status
                if (m_onTick) {               // host: model-status update
                    m_onTick();
                }
                // Reset one-frame guard before draining deferred actions
                // (deferred menu switch sets it; next iteration clears it)
                m_menuBar.endWait("wait cleared, a pass had events");
                m_windowManager.drainDeferred();
            }

            // A toolbar tooltip waiting out its delay: a resting pointer sends no
            // event to show it on
            m_windowManager.showDueTooltip();

            // A glide's next frame: no event comes to draw it on either
            m_windowManager.keepGliding();

            // The Ctrl+Tab list, once Ctrl has been held past its delay: a held key sends none
            m_tabSwitcher.showIfDue();

            if (m_windowManager.renderQueue().hasPending()) {
                m_windowManager.renderAll();
                // A list made before this frame has had it, so a switch may take it down -
                // with no event after it, nothing else would say so
                m_menuBar.endWait("wait cleared, a frame was drawn");
                m_windowManager.drainDeferred();
            }
            m_menuBar.replayHeldHover();

            if (isStopAfterFrame) {
                isStopAfterFrame = false;
                (void)std::raise(SIGINT);
            }

            // Idle until one refresh of the display after this pass began: a frame the host
            // asks for between passes (an animation) comes at the display's pace, never
            // faster than it can show one, and an idle loop polls no more often
            if (!m_windowManager.hasPendingEvents() && !m_windowManager.renderQueue().hasPending()) {
                m_windowManager.flush();
                Common::Time::sleepUntil(passStart + g_config.refreshPeriod());
            }
        }
    }

private:
    // Set, the run stops itself after its first frame (see run)
    static constexpr std::string_view STOP_AFTER_FRAME_VARIABLE = "PUREGLUI_STOP_AFTER_FRAME";

    Ui::Res::ResManager       m_resManager;
    Ui::Window::WindowManager m_windowManager { m_resManager };
    Ui::Render::Context       m_context { m_resManager };
    Ui::Action::Registry      m_actions;

    // Host domain steps, kept from initialize() so every reload replays them.
    Ui::init_hooks_t m_hooks;

    Ui::task_fn_t m_frameTasks;
    Ui::task_fn_t m_onTick;

    // Domain hooks (host-provided; empty on a standalone shell).
    std::function<void(id_t)> m_onTabActivated;
    std::function<void(id_t)> m_onTabClosed;
    std::function<void(id_t)> m_onDockRowActivated;
    std::function<void(id_t)> m_onDockRowToggled;
    Ui::predicate_fn_t        m_onKeyPress;

    Ui::MenuBar     m_menuBar { m_resManager, m_windowManager };     // the open popup menu and the pointer on the bar
    Ui::StatusLine  m_statusLine { m_resManager, m_windowManager };  // the status text and a temp message over it
    Ui::TabSwitcher m_tabSwitcher { m_resManager, m_windowManager }; // the Ctrl+Tab list while Ctrl is held

    // Per-extension file loaders (host-registered; empty on a standalone shell,
    // so OpenFile warns for every file type).
    std::unordered_map<std::string, Ui::action_fn_t> m_fileHandlers;
    Ui::Io::FileDialog                               m_fileDialog; // OpenFile's, asked after each pass

    // Window dimensions (cached from resize events).
    fpx_t m_windowWidth  = 0;
    fpx_t m_windowHeight = 0;

    // Bool flags (grouped for minimal padding).
    bool m_pendingResize    = false;
    bool m_isRescalePending = false; // a reload for a new DPI waits for its batch to end

    // ---- the load cycle: the one ordering that must never differ between
    // startup and reload. Split in two because loadAll() has to precede window
    // creation (WindowManager reads layout/theme/dock state from ResManager)
    // while the gate needs a live GL context to query capabilities.

    // First half: framework resources, then the host's own.
    void loadAllResources()
    {
        m_resManager.loadAll();
        if (m_hooks.loadDomainResources) {
            m_hooks.loadDomainResources();
        }
    }

    // Second half: the host disables what this machine cannot do, then the
    // unhandled-item pass runs. The pass is LAST because it also collapses a
    // parent whose every child ended up disabled, so it has to observe every
    // enabled-write the gate made - reverse the two and the same state yields
    // different chrome.
    void gateAndDisableUnhandled()
    {
        if (m_hooks.gateFeatures) {
            m_hooks.gateFeatures();
        }
        disableUnhandledItems();
    }

    // The files OpenFile's dialog picked, once it has closed: Linux's stays open beside the loop
    void takePickedFiles()
    {
        const std::optional<std::vector<std::string>> files = m_fileDialog.takeFiles();
        // On X11 the desktop holds the focus back from the portal's dialog, so it is handed over
        m_windowManager.setDialogAwaited(m_fileDialog.isOpen());
        if (files.has_value()) {
            Ui::Action::openPickedFiles(*this, *files);
        }
    }

    // A key while the Ctrl+Tab list is open, Ctrl held: the key res binds to OpenTabSwitcher
    // steps it as that binding says, Down steps down and Up up, Return switches, Escape takes
    // it down - and any other does nothing, so a shortcut behind it (Ctrl+Up, Ctrl+W) never
    // runs. A key with Ctrl up switches first,
    // its release having gone unseen, and goes on as a key; Escape then takes it down.
    // Whether the key was the list's
    bool handleTabSwitcherKey(const Ui::Window::Event & event, const std::string & keyStr)
    {
        if (!event.key.modifiers.has(Ui::Window::KeyModifier::Control)) {
            if (keyStr == "Escape") {
                m_tabSwitcher.cancel();
                return true;
            }
            commitTabSwitcher();
            return false;
        }
        if (keyStr == "Ctrl+Down") {
            m_tabSwitcher.cycle(1);
        } else if (keyStr == "Ctrl+Up") {
            m_tabSwitcher.cycle(-1);
        } else if (keyStr == "Ctrl+Return") {
            commitTabSwitcher();
        } else if (keyStr == "Ctrl+Escape") {
            m_tabSwitcher.cancel();
        } else {
            for (const Ui::intent_t & intent : m_context.mapKey(normalizeKeyString(keyStr)).intents) {
                if (intent.actionKey == Ui::Action::OPEN_TAB_SWITCHER) {
                    execute(intent);
                }
            }
        }
        return true;
    }

    // How many tabs the strip shows however scrolled, at the window's width now - what the layout
    // shows them by (UiLayout::buildWorkspaceTab), so the arrows and a reveal step as it draws
    [[nodiscard]] std::size_t tabsShown() const
    {
        const Ui::Res::Type::layout_t & layout = m_resManager.layout();
        return Ui::Render::tabsShownOf(Ui::Render::tabStripWidthOf(layout, toCss(m_windowManager.windowWidth())),
                                       layout.tabMinWidth,
                                       layout.tabArrowWidth,
                                       m_resManager.tabBar().count());
    }

    // Switches to what the Ctrl+Tab list has highlighted, as a click on its tab does -
    // nothing for the active one, or one closed since it opened
    void commitTabSwitcher()
    {
        const id_t tabId = m_tabSwitcher.takeChosenId();
        if (tabId != Ui::INVALID_ID) {
            setActiveTab(tabId);
        }
    }

    void handleKeyPress(const Ui::Window::Event & event)
    {
        // Convert key event to shortcut string
        const std::string keyStr = event.toShortcutString();

        if (keyStr.empty()) {
            return;
        }

        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Key pressed: " << keyStr << std::endl;
        }

        // The Ctrl+Tab list's keys come before every other while it is open
        if (m_tabSwitcher.isOpen() && handleTabSwitcherKey(event, keyStr)) {
            return;
        }

        // Dialog keyboard navigation (Escape, Return, Left, Right)
        if (m_windowManager.dialog().isOpen()) {
            if (keyStr == "Escape" || keyStr == "Return" || keyStr == "Left" || keyStr == "Right") {
                m_windowManager.dialog().keyPress(keyStr);
                return;
            }
            // Let global shortcuts (Ctrl+R, Ctrl+Q, etc.) pass through
        }

        // Escape closes an open popup menu (no dialog is showing here).
        if (keyStr == "Escape" && m_menuBar.isOpen()) {
            m_menuBar.close();
            return;
        }

        // Let the host consume domain shortcuts (e.g. mouse-rotation toggle)
        // before the generic shortcut->intent mapping.
        if (m_onKeyPress && m_onKeyPress(keyStr)) {
            return;
        }

        // Normalize key string to match the format in shortcuts map (lowercase, no spaces)
        const std::string normalizedKey = normalizeKeyString(keyStr);

        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Normalized key: " << normalizedKey << std::endl;
        }

        // Resolve the shortcut to intents via the Context facade, then execute.
        const Ui::result_t result = m_context.mapKey(normalizedKey);
        for (const Ui::intent_t & intent : result.intents) {
            execute(intent);
        }
    }

    // Normalize key string to match shortcuts map format (lowercase, no spaces)
    static std::string normalizeKeyString(const std::string & keyStr)
    {
        std::string result;
        result.reserve(keyStr.size());
        for (const char ch : keyStr) {
            if (std::isspace(static_cast<unsigned char>(ch)) == 0) {
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
        }
        return result;
    }

    // Map a resolved main-UI click to intents via the Context facade and execute
    // them, unless the element is chrome the framework services itself.
    void dispatchClick(const Ui::Render::element_event_t & click)
    {
        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] Click: type=" << Ui::Render::toInt(click.type) << " id=" << click.id << std::endl;
        }

        Ui::Render::RenderScope scope = Ui::Render::RenderScope::None;
        if (handleInternalChrome(click, scope)) {
            applyScope(scope);
            return;
        }

        const Ui::result_t result = m_context.mapClick(click, m_menuBar.openId());
        for (const Ui::intent_t & intent : result.intents) {
            execute(intent);
        }
        applyScope(Ui::Render::Context::scopeFor(click));
    }

    // The mirror of defaultBinding returning false: that says an element raises
    // no intent, this says what happens instead. Only the shell can do this work
    // - Context and WindowManager both hold a const ResManager &
    [[nodiscard]] bool handleInternalChrome(const Ui::Render::element_event_t & click,
                                            Ui::Render::RenderScope &           outScope)
    {
        switch (click.type) {
        case Ui::Render::UiElementType::TabArrow:
            if (click.id == Ui::TAB_ARROW_LEFT) {
                m_resManager.tabBar().scrollLeft(tabsShown());
            } else {
                m_resManager.tabBar().scrollRight(tabsShown());
            }
            outScope = Ui::Render::RenderScope::Layout;
            return true;
        case Ui::Render::UiElementType::MenuButton:
        case Ui::Render::UiElementType::ToolbarButton:
        case Ui::Render::UiElementType::MenuItem:
        case Ui::Render::UiElementType::Separator:
        case Ui::Render::UiElementType::Text:
        case Ui::Render::UiElementType::Image:
        case Ui::Render::UiElementType::Tab:
        case Ui::Render::UiElementType::TabClose:
        case Ui::Render::UiElementType::DockRow:
        case Ui::Render::UiElementType::DockExpander:
        case Ui::Render::UiElementType::DockGrip:
        case Ui::Render::UiElementType::DockSlider:
        case Ui::Render::UiElementType::DockScrollbar:
        case Ui::Render::UiElementType::DockMenu: return false;
        }
        return false;
    }

    // Turn a declared repaint class into the matching request. An intent may
    // already have asked for a repaint of its own; both paths are idempotent
    // (a queue insert and a dirty flag), so the overlap is free.
    void applyScope(Ui::Render::RenderScope scope)
    {
        switch (scope) {
        case Ui::Render::RenderScope::Layout: m_windowManager.requestContentRefresh(); break;
        case Ui::Render::RenderScope::Chrome: m_windowManager.requestMainRender(); break;
        case Ui::Render::RenderScope::None: break;
        }
    }

    // Execute one intent emitted by the Context facade. The intent -> chrome
    // command routing lives in Ui::routeIntent (shared with tests); the commands
    // themselves are the IChromeCommands overrides in the public section
    void execute(const Ui::intent_t & intent) { Ui::routeIntent(intent, *this); }

    void handleResize()
    {
        if constexpr (SHELL_DEBUG) {
            std::cout << "[Shell] handleResize(): " << m_windowWidth << "x" << m_windowHeight << std::endl;
        }

        // Update window dimensions (so mainWindow().bound() returns new size)
        m_windowManager.onMainWindowResize(m_windowWidth, m_windowHeight);

        // Force EGL surface to resize by doing a
        // swap - on Mesa/EGL X11 the surface resize
        // is deferred until eglSwapBuffers.
        m_windowManager.swapAfterResize();

        // Rebuild UI for new dimensions and render+swap synchronously.
        // Without this, the next main swap happens in the next main-loop iteration,
        // leaving the old UI (rendered for the old size) visible until then. With
        // WS_CLIPCHILDREN, the content surface's new rect is correctly punched out, but
        // the old UI fills the strip between the child's new edge and the new window
        // edge with workspace-bg - the right toolbar / status bar are drawn at their
        // old off-screen positions. That strip is the visible gap.
        m_windowManager.requestContentRefresh();
        m_windowManager.renderAll();

        // Recreate the popup at its new anchor (preserving submenu + active item,
        // same as a resource reload)
        m_menuBar.reopenAfterResize();

        m_pendingResize = false;
    }
};

} // namespace Ui
