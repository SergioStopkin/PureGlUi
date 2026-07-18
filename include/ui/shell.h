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

#include "common/cstr.h"
#include "common/noncopyable.h"
#include "common/system.h"
#include "common/unicode.h"
#include "ui/action/actionmap.h"
#include "ui/action/registry.h"
#include "ui/gl/localglew.h"
#include "ui/gl/svgrenderer.h"
#include "ui/intent.h"
#include "ui/interface/ichromecommands.h"
#include "ui/pubsub/subscribeid.h"
#include "ui/render/context.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/res/resmanager.h"
#include "ui/res/util.h"
#include "ui/result.h"
#include "ui/type.h"
#include "ui/window/event.h"
#include "ui/window/windowmanager.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
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
 * and all interactive-chrome orchestration (popup/menu lifecycle, dialog close,
 * temp status). A host composes a Shell, adds content +
 * domain actions, and wires the domain hooks (tab activate/close, key press).
 */
class Shell final : private Common::NonCopyable, private Ui::IChromeCommands {
public:
    Shell() { std::cout << "Hello!" << std::endl; }
    ~Shell() override { std::cout << "Bye!" << std::endl; }

    [[nodiscard]] Ui::Res::ResManager &       resManager() { return m_resManager; }
    [[nodiscard]] const Ui::Res::ResManager & resManager() const { return m_resManager; }

    [[nodiscard]] Ui::Window::WindowManager &       windowManager() { return m_windowManager; }
    [[nodiscard]] const Ui::Window::WindowManager & windowManager() const { return m_windowManager; }

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

    // Reload-with-open-chrome: remember any open dialog/menu, close it, run the
    // host's resource reload (it applies Changed + refreshes content), then
    // reopen what was open. The save/close/reopen is generic chrome; the host
    // supplies only the domain reload step. The open menu is remembered as a
    // single hierarchical key (the deepest active node), since numeric ids are
    // reassigned by loadAll() but label-derived keys survive it.
    void reloadChrome(const Ui::task_fn_t & reloadResources)
    {
        const bool                    hadDialog   = m_windowManager.hasDialog();
        const Ui::Res::Type::dialog_t savedDialog = hadDialog ? m_windowManager.lastDialogData()
                                                              : Ui::Res::Type::dialog_t {};
        const Ui::key_t               activeKey   = activeMenuKey();

        // Close open popup/dialog
        if (hadDialog) {
            m_windowManager.closeDialog();
        }
        if (m_openMenuId != Ui::INVALID_ID) {
            m_openMenuId = Ui::INVALID_ID;
            m_resManager.clearActiveMenu();
            destroyPopup();
        }

        if (reloadResources) {
            reloadResources();
        }

        // Re-query display DPI: the environment may have changed since init
        // (system zoom, monitor move) and every CSS->physical conversion below
        // (popup geometry, dock margins, fonts) reads g_config.scale. On a
        // scale change, run the same-size resize cascade so the renderer and
        // content surfaces pick up the new pixel math before anything reopens.
        if (m_windowManager.refreshDisplayMetrics()) {
            m_windowManager.onMainWindowResize(m_windowManager.windowWidth(), m_windowManager.windowHeight());
        }

        // Reopen what was open
        if (hadDialog) {
            m_windowManager.openDialog(savedDialog);
        } else if (!activeKey.empty()) {
            restoreActiveMenu(activeKey);
        }
    }

    // Hierarchical key of the deepest active menu node: the highlighted item
    // (across popup + submenu) if any, else the open dropdown. "" = nothing open.
    [[nodiscard]] Ui::key_t activeMenuKey() const
    {
        if (m_openMenuId == Ui::INVALID_ID) {
            return {};
        }
        const id_t activeItem = m_windowManager.activePopupItemId();
        const id_t node       = (activeItem != Ui::INVALID_ID) ? activeItem : m_openMenuId;
        return m_resManager.findMenuItem(node).key;
    }

    // Reopen the dropdown + any submenu named by a hierarchical key ("File",
    // "View:Theme", "View:Theme:default") and re-highlight the leaf node.
    void restoreActiveMenu(const Ui::key_t & activeKey)
    {
        const std::vector<Ui::key_t> segments = Ui::Res::Util::splitMenuKey(activeKey);
        if (segments.empty()) {
            return;
        }

        const Ui::Res::Type::menu_t dropdown = m_resManager.findMenuItemByKey(segments.front());
        if (dropdown.id == Ui::INVALID_ID) {
            return;
        }
        createMenuPopup(dropdown.id);

        // A node two levels deep that has children is the submenu to reopen
        // (covers both a hovered submenu-parent and a hovered submenu-child).
        if (segments.size() >= 2) {
            const Ui::key_t             parentKey = segments[0] + ":" + segments[1];
            const Ui::Res::Type::menu_t parent    = m_resManager.findMenuItemByKey(parentKey);
            if (parent.id != Ui::INVALID_ID && !parent.items.empty()) {
                m_windowManager.reopenSubmenu(parent.id);
            }
        }

        const Ui::Res::Type::menu_t leaf = m_resManager.findMenuItemByKey(activeKey);
        if (leaf.id != Ui::INVALID_ID) {
            m_windowManager.setActivePopupItem(leaf.id);
        }
    }

    // Recreate the open popup (+ submenu + active-item highlight) at its anchored
    // position - e.g. after a window move/resize relocates or rebuilds the popup
    // window. Mirrors the menu-state preservation reloadChrome does across a
    // resource reload, so the highlighted item survives all three.
    void reopenActivePopup()
    {
        const Ui::key_t activeKey = activeMenuKey();
        if (activeKey.empty()) {
            return;
        }
        destroyPopup();
        restoreActiveMenu(activeKey);
    }

    // Close any open popup menu (no-op if none). Generic chrome; the host calls
    // it before opening a native modal (e.g. the file dialog).
    void closePopupMenu()
    {
        if (m_openMenuId != Ui::INVALID_ID) {
            destroyPopup();
        }
    }

    // True while a temporary (auto-expiring) status message is showing. The host
    // model-status updater checks this so it does not clobber a temp message.
    [[nodiscard]] bool hasTempStatus() const { return !m_tempStatusText.empty(); }

    // --- init spine (each step is generic; a host interleaves its domain steps) ---

    void loadResources() { m_resManager.loadAll(); }

    [[nodiscard]] bool initWindow() { return m_windowManager.initialize(); }

    // Preload the SVG icons referenced by button definitions.
    void preloadButtonIcons()
    {
        size_t preloaded = 0;
        for (const auto & button : m_resManager.buttons()) {
            if (button.icon.empty()) {
                continue;
            }
            const std::string path = button.icon.find('/') == std::string::npos
                                   ? m_resManager.resPath().icon(button.icon)
                                   : button.icon;
            if (!Ui::Gl::SvgRenderer::loadFromFile(path).empty()) {
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

    // Grey out leaf menu items whose actionKey has no registered handler, so the
    // chrome never offers a click that does nothing. Call once after all actions
    // are registered (shell defaults + any host domain actions). Dialog/submenu
    // items stay enabled - the shell drives them without an Action::Registry entry.
    void disableUnhandledMenuItems()
    {
        m_resManager.disableUnhandledMenuItems([this](const std::string & key) { return m_actions.has(key); });
    }

    // Run the full generic init spine (standalone shell convenience). A host
    // that interleaves domain steps does NOT call this - it drives the granular
    // pieces itself, calls wireEvents() + registers its domain actions, then
    // calls disableUnhandledMenuItems() at the right point.
    //
    // afterLoadResources (optional) runs once after resources load but before the
    // window is created - the point at which a host restores persisted state
    // (theme, window geometry) so the first window picks it up.
    [[nodiscard]] bool initialize(const Ui::task_fn_t & afterLoadResources = {})
    {
        loadResources();
        if (afterLoadResources) {
            afterLoadResources();
        }
        if (!initWindow()) {
            return false;
        }
        m_windowManager.mainWindow().makeCurrent();
        preloadButtonIcons();
        initRenderer();
        wireEvents();
        disableUnhandledMenuItems();
        return true;
    }

    // Register the generic OS-event subscriptions, hover wiring, default shell
    // actions, and the dialog-close handler. Host calls this once after the
    // renderer exists; standalone initialize() calls it too.
    void wireEvents()
    {
        auto & sub = m_windowManager.subscribe();

        // CloseRequested
        sub.add(Ui::PubSub::eventSourceId(Ui::Window::EventType::CloseRequested),
                Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
                [this]() {
                    std::cout << "[Event] Window close requested" << std::endl;
                    m_windowManager.stop();
                });

        // Resize
        sub.add(
        Ui::PubSub::eventSourceId(Ui::Window::EventType::Resize),
        Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
        [this]() {
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
            m_resManager.setSessionWindowGeometry(frameX, frameY, static_cast<int>(newW), static_cast<int>(newH));

            if (newW != m_windowWidth || newH != m_windowHeight) {
                std::cout << "[Shell] Resize event: " << newW << "x" << newH << " (current: " << m_windowWidth << "x"
                          << m_windowHeight << ")" << std::endl;
                m_windowWidth  = newW;
                m_windowHeight = newH;

                // Hide popup during resize (recreated in handleResize). A child
                // popup tracks the parent itself, so leave it - hiding/recreating
                // it every resize event is what makes it blink.
                if (m_openMenuId != Ui::INVALID_ID && m_windowManager.popupWindow() != nullptr
                    && !m_windowManager.popupFollowsParent()) {
                    m_windowManager.popupWindow()->hide();
                }

                // Defer resize (only once per batch -- last dimensions win)
                if (!m_pendingResize) {
                    m_pendingResize = true;
                    m_windowManager.subscribe().defer([this]() { handleResize(); });
                }
            }
        });

        // MouseButtonPress
        sub.add(
        Ui::PubSub::eventSourceId(Ui::Window::EventType::MouseButtonPress),
        Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
        [this]() {
            if (m_windowManager.hasDialog()) {
                return;
            }

            const auto & event = m_windowManager.currentEvent();

            // Popup press -- only handle outside-click policy (inside forwarding done by dispatchEvent)
            if (event.isPopupEvent && m_windowManager.hasPopup()) {
                if (!m_windowManager.popupEventConsumed()) {
                    // Click outside popup visual region
                    bool shouldClosePopup = true;

                    int mainX = 0;
                    int mainY = 0;
                    m_windowManager.popupToMainCoords(event.mouse.x, event.mouse.y, mainX, mainY);

                    if (m_windowManager.hasUiRenderer()) {
                        const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
                        if (mainY >= 0 && mainY < menuBarHeight) {
                            if (mainX < menusTotalWidthPhysical()) {
                                const bool hitMenuItem = m_windowManager.onMousePress(mainX,
                                                                                      mainY,
                                                                                      event.mouse.clickCount);
                                m_windowManager.requestMainRender();
                                if (hitMenuItem) {
                                    shouldClosePopup = false;
                                    std::cout << "[Shell] Clicked menu item, switching popup" << std::endl;
                                } else {
                                    std::cout << "[Shell] Clicked menu bar between buttons - closing popup"
                                              << std::endl;
                                }
                            } else {
                                std::cout << "[Shell] Clicked menu bar empty space after menus (x=" << mainX << " > "
                                          << menusTotalWidthPhysical() << ") - closing popup" << std::endl;
                            }
                        } else {
                            if (event.mouse.x >= 0 && event.mouse.x < m_windowManager.popupWindow()->bound().w
                                && event.mouse.y >= 0 && event.mouse.y < m_windowManager.popupWindow()->bound().h) {
                                shouldClosePopup = false;
                            }
                        }
                    }

                    if (shouldClosePopup) {
                        std::cout << "[Shell] Closing popup (clicked outside menu and popup area)" << std::endl;
                        destroyPopup();
                        // Forward click to main window
                        if (event.mouse.button == Ui::Window::MouseButton::Left) {
                            m_windowManager.onMousePress(mainX, mainY, event.mouse.clickCount);
                            m_windowManager.requestMainRender();
                        }
                    }
                }
                return;
            }

            // Non-popup press -- close popup if clicking outside of it
            std::cout << "[Shell] MouseButtonPress event received at (" << event.mouse.x << "," << event.mouse.y << ")"
                      << std::endl;
            if (m_windowManager.hasPopup()) {
                bool      closePopup    = true;
                const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
                if (event.mouse.y >= 0 && event.mouse.y < menuBarHeight) {
                    if (event.mouse.x < menusTotalWidthPhysical()) {
                        std::cout << "[Shell] Click on menu button while popup open - deferring to click handler"
                                  << std::endl;
                        closePopup = false;
                    } else {
                        std::cout << "[Shell] Click in menu bar empty space (after menus at x=" << event.mouse.x
                                  << " > " << menusTotalWidthPhysical() << ") - closing popup" << std::endl;
                    }
                } else {
                    std::cout << "[Shell] Closing popup (clicked outside)" << std::endl;
                }
                if (closePopup) {
                    destroyPopup();
                }
            }
            // Main window press routing already done by dispatchEvent
            if (event.childWindowId == Ui::INVALID_ID) {
                m_windowManager.requestMainRender();
            }
        });

        // MouseButtonRelease
        sub.add(Ui::PubSub::eventSourceId(Ui::Window::EventType::MouseButtonRelease),
                Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
                [this]() {
                    if (m_windowManager.hasDialog()) {
                        return;
                    }

                    const auto & event = m_windowManager.currentEvent();

                    // Popup release -- handle click result or close
                    if (event.isPopupEvent && m_windowManager.hasPopup()) {
                        if (m_windowManager.popupEventConsumed()) {
                            const auto & result = m_windowManager.lastClickResult();
                            if (result.id != Ui::INVALID_ID) {
                                dispatchClick(result);
                            }
                        } else {
                            std::cout << "[Shell] Popup MouseButtonRelease outside visual region - closing popup"
                                      << std::endl;
                            destroyPopup();
                        }
                        return;
                    }

                    // Main/child release -- handle click result
                    const auto & result = m_windowManager.lastClickResult();
                    if (result.id != Ui::INVALID_ID) {
                        dispatchClick(result);
                    }
                    if (event.childWindowId == Ui::INVALID_ID) {
                        m_windowManager.requestMainRender();
                    }
                });

        // MouseMove
        sub.add(Ui::PubSub::eventSourceId(Ui::Window::EventType::MouseMove),
                Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
                [this]() {
                    if (m_windowManager.hasDialog()) {
                        return;
                    }

                    const auto & event = m_windowManager.currentEvent();

                    // Popup move -- handle menu bar hover detection
                    if (event.isPopupEvent) {
                        if (m_windowManager.hasUiRenderer() && m_windowManager.hasPopup()) {
                            int mainX = 0;
                            int mainY = 0;
                            m_windowManager.popupToMainCoords(event.mouse.x, event.mouse.y, mainX, mainY);

                            const int menuBarHeight = toPhysFloor(m_resManager.layout().topMenu.height);
                            if (mainY >= 0 && mainY < menuBarHeight) {
                                if (m_windowManager.onMouseMove(mainX, mainY)) {
                                    m_windowManager.requestMainRender();
                                }
                            }
                        }
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
                    if (m_windowManager.hasUiRenderer()) {
                        const fpx_t cssX = toCss(event.mouse.x);
                        const fpx_t cssY = toCss(event.mouse.y);
                        for (const auto & m : m_resManager.menus()) {
                            const auto & b = m_windowManager.elementBound(m.id);
                            if (b.w > 0 && b.contains(cssX, cssY)) {
                                if (m_hoveredMenuId != m.id) {
                                    m_hoveredMenuId = m.id;
                                    handleMenuHover(m.id);
                                }
                                break;
                            }
                        }
                    }
                });

        // KeyPress
        sub.add(Ui::PubSub::eventSourceId(Ui::Window::EventType::KeyPress),
                Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App),
                [this]() { handleKeyPress(m_windowManager.currentEvent()); });

        // Hover handler for menu switching.
        m_windowManager.setOnElementHover([this](Ui::Render::UiElementType type, id_t id) {
            (void)type;
            std::cout << "[Shell] Menu button hover: id=" << id << std::endl;
            handleMenuHover(id);
        });

        // Bind the built-in framework actions (ui/action/). A host registers its
        // own domain actions the same way (m_shell.actions().on(...)); keys with
        // no binding are greyed out by the disable pass.
        Ui::Action::registerActions(*this);

        // Dialog content placeholders (%VERSION%/%CPU%/%RAM%/%GPU%/%GL%) resolved
        // from APP_VERSION + Common::System + the main window's GL context.
        m_windowManager.setDialogContentResolver(
        [this](const std::string & tpl) { return resolveDialogPlaceholders(tpl); });

        // Dialog close: WindowManager invokes this (deferred), not a cross-thread event.
        m_windowManager.setOnDialogClose([this]() {
            const auto   action = m_windowManager.lastDialogAction();
            const auto & dialog = m_windowManager.lastDialogData();

            if (action == Ui::Res::Type::DialogAction::CopyLink && !dialog.link.empty()) {
                const std::string & linkText = m_resManager.localeManager().get(dialog.link);
                if (!linkText.empty() && m_windowManager.copyToClipboard(linkText)) {
                    showTempStatus("Copied to clipboard");
                }
            }

            // Close popup menu if still open
            if (m_openMenuId != Ui::INVALID_ID) {
                m_openMenuId = Ui::INVALID_ID;
                m_resManager.clearActiveMenu();
                m_windowManager.requestContentRefresh();
                destroyPopup();
            }

            std::cout << "[Shell] Dialog closed with result: " << static_cast<int>(action) << std::endl;
        });
    }

    // Expand %KEY% placeholders in dialog content (the System Info / Version
    // dialogs): %VERSION% from APP_VERSION, %CPU%/%RAM% from Common::System,
    // %GPU%/%GL% from the main window's live GL context.
    std::wstring resolveDialogPlaceholders(const std::string & tpl)
    {
        std::wstring result = Common::Unicode::fromUtf8(tpl);

        auto replace = [&result](const std::wstring & key, const std::wstring & value) {
            auto pos = result.find(key);
            while (pos != std::wstring::npos) {
                result.replace(pos, key.size(), value);
                pos = result.find(key, pos + value.size());
            }
        };

        replace(L"%VERSION%", L"" APP_VERSION);

        std::string       cpu  = std::to_string(Common::System::cpuCores()) + " cores";
        const std::string freq = Common::System::cpuFrequency();
        if (freq != "N/A") {
            cpu += " @ " + freq;
        }
        replace(L"%CPU%", Common::Unicode::fromUtf8(cpu));

        // GL info (requires current context)
        auto & mainWindow = m_windowManager.mainWindow();
        mainWindow.makeCurrent();
        const GLubyte *   gpu    = glGetString(GL_RENDERER);
        const GLubyte *   gl     = glGetString(GL_VERSION);
        const std::string gpuStr = gpu != nullptr ? Common::fromCString(gpu) : "N/A";
        std::string       glStr  = gl != nullptr ? Common::fromCString(gl) : "N/A";
        // isHardwareGl() is authoritative: WindowManager folds the GL_RENDERER
        // software-rasterizer check into it right after context creation.
        glStr += mainWindow.isHardwareGl() ? " (hardware)" : " (software)";
        replace(L"%GPU%", Common::Unicode::fromUtf8(gpuStr));
        replace(L"%GL%", Common::Unicode::fromUtf8(glStr));

        replace(L"%RAM%", Common::Unicode::fromUtf8(Common::System::systemRam()));

        return result;
    }

    // The main event loop: drain host frame tasks, poll+dispatch OS events,
    // run the generic per-frame upkeep + host tick, render the dirty set. Built
    // purely on WindowManager primitives, so it runs with or without content.
    void run()
    {
        while (m_windowManager.isRunning()) {
            if (m_frameTasks) {
                m_frameTasks();
            }

            bool              hadEvents = false;
            Ui::Window::Event event;
            while (m_windowManager.pollEvent(event)) {
                m_windowManager.dispatchEvent(event);
                hadEvents = true;
            }

            if (hadEvents) {
                handlePopupMove();       // popup-position tracking during window move
                checkTempStatusExpiry(); // expire + restore an active temp status
                if (m_onTick) {          // host: model-status update
                    m_onTick();
                }
                // Reset one-frame guard before draining deferred actions
                // (deferred menu switch sets it; next iteration clears it)
                if (m_menuJustActivated) {
                    m_menuJustActivated = false;
                }
                m_windowManager.drainDeferred();
            }

            if (m_windowManager.renderQueue().hasPending()) {
                m_windowManager.renderAll();
                m_windowManager.drainDeferred();
            }

            // Block until next event or timeout (periodic tasks like load progress)
            if (!m_windowManager.hasPendingEvents() && !m_windowManager.renderQueue().hasPending()) {
                m_windowManager.flush();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

private:
    Ui::Res::ResManager       m_resManager;
    Ui::Window::WindowManager m_windowManager { m_resManager };
    Ui::Render::Context       m_context { m_resManager };
    Ui::Action::Registry      m_actions;

    Ui::task_fn_t m_frameTasks;
    Ui::task_fn_t m_onTick;

    // Domain hooks (host-provided; empty on a standalone shell).
    std::function<void(id_t)> m_onTabActivated;
    std::function<void(id_t)> m_onTabClosed;
    Ui::predicate_fn_t        m_onKeyPress;

    // Per-extension file loaders (host-registered; empty on a standalone shell,
    // so OpenFile warns for every file type).
    std::unordered_map<std::string, Ui::action_fn_t> m_fileHandlers;

    // Temporary status bar message (auto-expires).
    std::chrono::steady_clock::time_point m_tempStatusExpiry;
    std::string                           m_tempStatusText;
    std::string                           m_lastStatusText; // real text to restore after a temp expires

    // Window dimensions (cached from resize events).
    fpx_t m_windowWidth  = 0;
    fpx_t m_windowHeight = 0;

    // Window position tracking (for popup repositioning).
    int m_lastWindowX = 0;
    int m_lastWindowY = 0;

    // Popup menu state.
    id_t m_openMenuId    = Ui::INVALID_ID; // ID of currently open popup menu (INVALID = none)
    id_t m_hoveredMenuId = Ui::INVALID_ID; // ID of menu currently hovered (avoids duplicate hover actions)

    // Bool flags (grouped for minimal padding).
    bool m_pendingResize     = false;
    bool m_menuSwitchPending = false; // Re-entry guard for deferred menu switch
    bool m_menuJustActivated = false; // True briefly after creating a popup until it renders once

    static constexpr std::chrono::seconds TEMP_STATUS_DURATION { 1 };

    // Total physical pixel width of all menu buttons.
    [[nodiscard]] int menusTotalWidthPhysical() const
    {
        if (!m_windowManager.hasUiRenderer()) {
            return 0;
        }

        fpx_t totalW = 0;
        for (const auto & menu : m_resManager.menus()) {
            const auto & bound = m_windowManager.elementBound(menu.id);
            totalW += bound.w;
        }

        return toPhysFloor(totalW);
    }

    // Track the popup while the main window moves: shift it by the window delta,
    // hide during the move, recreate once the move settles. Generic chrome.
    void handlePopupMove()
    {
        if (m_openMenuId == Ui::INVALID_ID || !m_windowManager.hasPopup()) {
            return;
        }
        // An X11 child popup is moved by the server with its parent; tracking it
        // here would double-move it. Only root-parented popups need manual moves.
        if (m_windowManager.popupFollowsParent()) {
            return;
        }
        int currentX = 0;
        int currentY = 0;
        m_windowManager.mainWindowScreenPosition(currentX, currentY);
        if (currentX == m_lastWindowX && currentY == m_lastWindowY) {
            return;
        }
        const int deltaX = currentX - m_lastWindowX;
        const int deltaY = currentY - m_lastWindowY;
        m_lastWindowX    = currentX;
        m_lastWindowY    = currentY;

        // Translate the popup (+ submenu) by the same delta as the main window
        // so they stay attached - no destroy/recreate. A rigid move keeps the
        // captured corner background valid (the parent pixels behind the corners
        // translate identically), which is why no recapture is needed.
        if (auto * popup = m_windowManager.popupWindow(); popup != nullptr) {
            popup->move(popup->bound().x + deltaX, popup->bound().y + deltaY);
        }
        if (auto * submenu = m_windowManager.submenuWindow(); submenu != nullptr) {
            submenu->move(submenu->bound().x + deltaX, submenu->bound().y + deltaY);
        }
    }

    // Expire an active temp status: restore the saved real text once the
    // duration elapses. hasTempStatus() reports whether one is still showing.
    void checkTempStatusExpiry()
    {
        if (!m_tempStatusText.empty() && std::chrono::steady_clock::now() >= m_tempStatusExpiry) {
            m_tempStatusText.clear();
            // Restore real status text
            m_resManager.setStatusText(m_lastStatusText);
            m_windowManager.requestContentRefresh();
        }
    }

    /**
     * @brief Helper to destroy popup and cleanup state
     * @param syncDisplay If true, sync with X server to ensure window is removed before returning
     */
    void destroyPopup(bool syncDisplay = false)
    {
        m_windowManager.destroyPopup(syncDisplay);

        m_openMenuId = Ui::INVALID_ID;
        m_resManager.clearActiveMenu();
        m_windowManager.requestContentRefresh();
    }

    void handleKeyPress(const Ui::Window::Event & event)
    {
        // Convert key event to shortcut string
        const std::string keyStr = event.toShortcutString();

        if (keyStr.empty()) {
            return;
        }

        std::cout << "[Shell] Key pressed: " << keyStr << std::endl;

        // Dialog keyboard navigation (Escape, Return, Left, Right)
        if (m_windowManager.hasDialog()) {
            if (keyStr == "Escape" || keyStr == "Return" || keyStr == "Left" || keyStr == "Right") {
                m_windowManager.dialogKeyPress(keyStr);
                return;
            }
            // Let global shortcuts (Ctrl+R, Ctrl+Q, etc.) pass through
        }

        // Escape closes an open popup menu (no dialog is showing here).
        if (keyStr == "Escape" && m_openMenuId != Ui::INVALID_ID) {
            closePopupMenu();
            return;
        }

        // Let the host consume domain shortcuts (e.g. mouse-rotation toggle)
        // before the generic shortcut->intent mapping.
        if (m_onKeyPress && m_onKeyPress(keyStr)) {
            return;
        }

        // Normalize key string to match the format in shortcuts map (lowercase, no spaces)
        const std::string normalizedKey = normalizeKeyString(keyStr);

        std::cout << "[Shell] Normalized key: " << normalizedKey << std::endl;

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

    void handleMenuHover(id_t menuId)
    {
        // Skip action menus (icon buttons with no popup)
        for (const auto & menu : m_resManager.menus()) {
            if (menu.id == menuId && !menu.actionKey.empty()) {
                return;
            }
        }

        // A disabled top menu has no usable dropdown: skip it. Leave the menu
        // state untouched - moving across a disabled menu keeps the currently-open
        // popup open (so moving back to it finds it still open) and never opens a
        // dropdown of its own.
        if (!m_resManager.findMenuItem(menuId).enabled) {
            return;
        }

        // If no menu is currently open, hovering should NOT open a popup --
        // a click is required to open the first menu. Just remember hovered id.
        if (m_openMenuId == Ui::INVALID_ID && !m_menuSwitchPending) {
            if (m_hoveredMenuId != menuId) {
                std::cout << "[Shell] Hovered menu (no popup open): " << menuId << std::endl;
                m_hoveredMenuId = menuId;
            }
            return;
        }

        // Don't switch to the same menu
        if (m_openMenuId == menuId) {
            return;
        }

        // Don't switch immediately after activation -- wait one render pass
        // to avoid destroying a popup that was just created this frame.
        if (m_menuJustActivated) {
            return;
        }

        // Always defer menu switching when hovering:
        // 1. Destroy old popup now (so the area it covered can be redrawn)
        // 2. Mark main window for re-render
        // 3. Create new popup AFTER render (via deferred mechanism)
        // This ensures corner capture sees the properly rendered background.
        if (!m_menuSwitchPending) {
            destroyPopup(true); // Destroy old popup, triggers requestContentRefresh
            m_menuSwitchPending = true;
            m_windowManager.subscribe().defer([this, menuId]() {
                std::cout << "[Shell] Committing deferred menu switch to: " << menuId << std::endl;
                createMenuPopup(menuId);
                m_menuSwitchPending = false;
                m_menuJustActivated = true;
            });
        }
    }

    void createMenuPopup(id_t menuId)
    {
        // Destroy any existing popup before creating a new one
        // Use syncDisplay=true to ensure old popup is removed before corner capture
        if (m_openMenuId != Ui::INVALID_ID) {
            destroyPopup(true);
        }

        m_openMenuId = menuId;
        m_resManager.setActiveMenu(menuId);
        m_windowManager.requestContentRefresh(); // Rebuild layout with active menu highlight next frame
        std::cout << "[Shell] Creating popup for menu: " << menuId << std::endl;

        // Find the menu
        const auto & menus = m_resManager.menus();
        auto         it = std::find_if(menus.begin(), menus.end(), [menuId](const auto & m) { return m.id == menuId; });

        if (it == menus.end()) {
            std::cerr << "[Shell] Menu not found: " << menuId << std::endl;
            return;
        }

        const auto & menu = *it;

        // Get main window's absolute screen position
        int mainWindowScreenX = 0;
        int mainWindowScreenY = 0;
        m_windowManager.mainWindowScreenPosition(mainWindowScreenX, mainWindowScreenY);

        // Store current window position for move detection
        m_lastWindowX = mainWindowScreenX;
        m_lastWindowY = mainWindowScreenY;

        // Calculate cumulative X position by summing widths of all preceding menu buttons
        fpx_t buttonXCss = 0;
        fpx_t buttonH    = 0;
        if (m_windowManager.hasUiRenderer()) {
            for (const auto & m : menus) {
                const auto & b = m_windowManager.elementBound(m.id);
                if (m.order < menu.order) {
                    buttonXCss += b.w;
                } else if (m.id == menuId) {
                    buttonH = b.h;
                    break;
                }
            }
        }
        // Y = bottom of menu button (button height in CSS pixels)
        const fpx_t buttonYCss = buttonH;

        std::cout << "[Shell] Popup position: menuId=" << menuId << " X=" << buttonXCss << " Y=" << buttonYCss
                  << std::endl;

        // Use toPhysRound so position and size stay divisible by g_config.scale - i.e.
        // integer NS points on macOS (bsf=2). Fractional physical px produce a
        // half-point NS frame; the CAShapeLayer mask then snaps to integer points
        // while GL renders at the fractional edge, leaving a 1-px hairline at the
        // top/right.
        const fpx_t buttonX    = mainWindowScreenX + toPhysRound(buttonXCss);
        const fpx_t buttonY    = mainWindowScreenY + toPhysRound(buttonYCss);
        const fpx_t popupCssW  = m_resManager.layout().topMenuDropdown.width;
        const fpx_t popupWidth = toPhysRound(popupCssW);

        const fpx_t contentCssH = menu.popupHeight;
        const fpx_t popupHeight = toPhysRound(contentCssH);

        std::cout << "[Shell] Popup height calc: contentCssH=" << contentCssH << " scaled=" << popupHeight << std::endl;

        const Ui::Res::Type::border_t popupRadii = m_resManager.layout().topMenuDropdown.border.scaled(g_config.scale);

        const Ui::Res::Type::bound_t popupBound = { buttonX, buttonY, popupWidth, popupHeight };
        if (!m_windowManager.createPopup(popupBound, popupRadii, m_resManager.theme().second.bg)) {
            std::cerr << "[Shell] Failed to create popup window" << std::endl;
            return;
        }

        auto * popupWindow = m_windowManager.popupWindow();
        if (popupWindow == nullptr) {
            std::cerr << "[Shell] Popup window not available" << std::endl;
            return;
        }

        std::cout << "[Shell] Popup size: " << popupWidth << "x" << popupHeight << std::endl;

        // Initialize renderer with corner capture (renders main window, captures corners)
        m_windowManager.initPopupRenderer();

        // Render first frame before showing to avoid visible flat/unrounded flash
        if (m_windowManager.renderPopup()) {
            if (g_config.isCompositing) {
                m_windowManager.capturePopupPixels();
                m_windowManager.refreshComposite();
            } else {
                popupWindow->swapBuffers();
            }
        }

        if (!g_config.isCompositing) {
            popupWindow->show();
        }

        // Mark that this popup was just activated; we want to wait one render pass
        // before honoring hover-triggered switches to avoid quick reentrancy.
        m_menuJustActivated = true;

        // Restore main window GL context after popup setup
        // (popup creation switches to popup context)
        m_windowManager.mainWindow().makeCurrent();

        std::cout << "[Shell] Popup renderer initialized successfully" << std::endl;
    }

    // Map a resolved main-UI click to intents via the Context facade and execute
    // them. TabArrow scroll is fw-internal (no host action), so it is handled
    // here directly rather than as an intent.
    void dispatchClick(const Ui::Render::click_result_t & click)
    {
        std::cout << "[Shell] Click: type=" << Ui::Render::toInt(click.type) << " id=" << click.id << std::endl;

        if (click.type == Ui::Render::UiElementType::TabArrow) {
            if (click.id == Ui::Render::TAB_ARROW_LEFT) {
                m_resManager.tabBar().scrollLeft();
            } else {
                m_resManager.tabBar().scrollRight();
            }
            m_windowManager.requestContentRefresh();
            return;
        }

        const Ui::result_t result = m_context.mapClick(click, m_openMenuId);
        for (const Ui::intent_t & intent : result.intents) {
            execute(intent);
        }
    }

    // Execute one intent emitted by the Context facade. The intent -> chrome
    // command routing lives in Ui::routeIntent (shared with tests); the commands
    // themselves are the private IChromeCommands overrides below.
    void execute(const Ui::intent_t & intent) { Ui::routeIntent(intent, *this); }

    // -- IChromeCommands: the window/chrome commands routeIntent() drives. Each
    // mirrors the branch it replaced in the former execute() switch. Domain-coupled
    // ones (tab activate/close) fire host hooks.
    void emitAction(const std::string & actionKey, const std::string & arg) override
    {
        // arg is the item label (value) for parameterized actions, "" otherwise.
        m_actions.dispatch(actionKey, arg);
    }

    [[nodiscard]] bool isPopupOpen() const override { return m_openMenuId != Ui::INVALID_ID; }

    void openPopup(id_t menuId) override { createMenuPopup(menuId); }

    void closePopup() override
    {
        // Deferred: the click may originate inside the popup window we are about to
        // destroy, so close after the current event drains. routeIntent has already
        // gated this on isPopupOpen().
        m_openMenuId = Ui::INVALID_ID;
        m_resManager.clearActiveMenu();
        m_windowManager.requestContentRefresh();
        m_windowManager.subscribe().defer([this]() { destroyPopup(); });
    }

    void openDialog(id_t itemId) override
    {
        const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(itemId);
        m_hoveredMenuId                  = Ui::INVALID_ID;
        m_windowManager.onMouseLeave();
        m_windowManager.subscribe().defer([this, dialog = item.dialog]() { m_windowManager.openDialog(dialog); });
    }

    void switchTab(id_t tabId) override
    {
        if (m_onTabActivated) {
            m_onTabActivated(tabId);
        }
    }

    void closeTab(id_t tabId) override
    {
        if (m_onTabClosed) {
            m_onTabClosed(tabId);
        }
    }

    void copyText(const std::string & text) override
    {
        if (m_windowManager.copyToClipboard(text)) {
            showTempStatus("Copied to clipboard");
        }
    }

    void handleResize()
    {
        std::cout << "[Shell] handleResize(): " << m_windowWidth << "x" << m_windowHeight << std::endl;

        // Update window dimensions (so mainWindow().bound() returns new size)
        m_windowManager.onMainWindowResize(m_windowWidth, m_windowHeight);

        // Force EGL surface to resize by doing a
        // swap - on Mesa/EGL X11 the surface resize
        // is deferred until eglSwapBuffers.
        m_windowManager.flushMainWindow();

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
        // same as a resource reload) - but only root-parented popups, since a
        // child popup is re-anchored by the server and recreating it would blink.
        if (m_openMenuId != Ui::INVALID_ID && m_windowManager.hasPopup() && !m_windowManager.popupFollowsParent()) {
            reopenActivePopup();
        }

        m_pendingResize = false;
    }

    // Show a temporary (auto-expiring) status message. Saves the current real
    // status text first so checkTempStatusExpiry() can restore it on expiry.
    void showTempStatus(const std::string & text)
    {
        m_lastStatusText   = m_resManager.statusText();
        m_tempStatusText   = text;
        m_tempStatusExpiry = std::chrono::steady_clock::now() + TEMP_STATUS_DURATION;
        m_resManager.setStatusText(text);
        m_windowManager.requestContentRefresh();
    }
};

} // namespace Ui
