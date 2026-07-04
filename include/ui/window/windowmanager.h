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

#include "common/bit.h"
#include "common/noncopyable.h"
#include "common/sanitize.h"
#include "common/unicode.h"
#include "ui/config.h"
#include "ui/gl/glutil.h"
#include "ui/gl/svgrenderer.h"
#include "ui/interface/ieventapp.h"
#include "ui/interface/ieventos.h"
#include "ui/interface/irenderer.h"
#include "ui/interface/iwindow.h"
#include "ui/pubsub/subscribe.h"
#include "ui/pubsub/subscribeid.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/popup/popuprenderer.h"
#include "ui/render/uirenderer.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/changed.h"
#include "ui/type.h"
#include "ui/window/compositetexture.h"
#include "ui/window/contenthit.h"
#include "ui/window/contentsurface.h"
#include "ui/window/event.h"
#include "ui/window/eventfactory.h"
#include "ui/window/nativewindow.h"
#include "ui/window/popup/dialogwindow.h"
#include "ui/window/popup/popupwindow.h"
#include "ui/window/renderqueue.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
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

// Flip to true to trace how raw OS mouse events get routed to main/popup/content surface.
// Pair with WIN32_EVENT_DEBUG to see the full press->classify->dispatch chain.
constexpr bool WM_ROUTE_DEBUG = true;

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
    std::unique_ptr<NativeWindow> m_mainWindow;
    const Ui::Res::ResManager &   m_resManager;           // non-owning; lifetime guaranteed by the owner (Shell/host)
    Ui::Render::UiRenderer *      m_uiRenderer = nullptr; // non-owning; mainWindow owns via setRenderer()

    // Dock columns (left/right collapsible panels). Keyed by dockSourceId
    // so the map key doubles as the Subscribe source id for state-change
    // events. Built in initialize() from m_resManager.layout().docks and
    // rebuilt in apply() on Ui::Res::Type::Changed::Layout (resource reload picks up new
    // or removed dock files).
    std::unordered_map<id_t, Ui::Render::DockColumn> m_docks;

    // Capture target while a grip drag is in flight. Ui::INVALID_ID = no drag.
    // While active, every mouse move/up routes exclusively to that dock,
    // even when the cursor wanders into the content surface or another area.
    id_t m_activeDragDock = Ui::INVALID_ID;

    // Popup window for dropdowns
    std::unique_ptr<Ui::Window::Popup::PopupWindow> m_popupWindow {};

    // Submenu popup window (child of m_popupWindow)
    std::unique_ptr<Ui::Window::Popup::PopupWindow> m_submenuWindow {};
    id_t                                            m_submenuItemId = Ui::INVALID_ID;

    // The menu/submenu windows both own a PopupRenderer; resolve it (nullptr if
    // the window or its renderer is not yet created).
    [[nodiscard]] Ui::Render::Popup::PopupRenderer * popupRenderer() const
    {
        return (m_popupWindow && m_popupWindow->hasRenderer()) ? &m_popupWindow->menuRenderer() : nullptr;
    }

    [[nodiscard]] Ui::Render::Popup::PopupRenderer * submenuRenderer() const
    {
        return (m_submenuWindow && m_submenuWindow->hasRenderer()) ? &m_submenuWindow->menuRenderer() : nullptr;
    }

    // Dialog window (modal)
    std::unique_ptr<Ui::Window::Popup::DialogWindow> m_dialogWindow {};
    Ui::Res::Type::DialogAction                      m_lastDialogResult = Ui::Res::Type::DialogAction::None;
    Ui::Res::Type::dialog_t                          m_lastDialogData;

    CompositeTexture m_popupComposite;
    CompositeTexture m_submenuComposite;
    CompositeTexture m_dialogComposite;

    // Content surfaces by id (the id doubles as the WsBase render-queue source).
    // m_activeContent is the only visible/interactive one (children overlap).
    std::unordered_map<id_t, content_surface_t> m_contentSurfaces;
    id_t                                        m_activeContent = Ui::INVALID_ID;

    // UI margins for child windows
    fpx_t m_uiLeft   = 0;
    fpx_t m_uiTop    = 0;
    fpx_t m_uiRight  = 0;
    fpx_t m_uiBottom = 0;

    // Window dimensions (set from caller in initialize())
    fpx_t m_windowWidth  = 0;
    fpx_t m_windowHeight = 0;

    // Composite texture shader (draws content/popup textures onto main window)
    GLuint m_compositeProgram     = 0;
    GLuint m_compositeVao         = 0;
    GLuint m_compositeVbo         = 0;
    GLint  m_compositeUProjection = -1;
    GLint  m_compositeUTex        = -1;

    std::atomic<bool> m_running { false };

    // Platform-specific event handler (compile-time detected)
    std::unique_ptr<NativeEvent> m_eventHandler {};

    // Content-changed subscriber system
    Ui::PubSub::Subscribe   m_subscribe;
    Ui::Window::RenderQueue m_renderQueue;

    // Content refresh: marks the main UI dirty for the next frame.
    bool m_contentIsDirty = false;

    // Host callback invoked (deferred) when a dialog closes.
    Ui::task_fn_t m_onDialogClose;

    // Host resolver for dialog content placeholders (%VERSION%/%CPU%/... ->
    // runtime values). The fw stays content-agnostic; if unset, the locale
    // string is shown verbatim.
    std::function<std::wstring(const std::string &)> m_dialogContentResolver;

    static constexpr id_t MAIN_WINDOW_ID = Ui::PubSub::sourceId(Ui::PubSub::SourceId::MainWindow);
    static constexpr id_t WS_GROUP_ID    = Ui::PubSub::sourceId(Ui::PubSub::SourceId::WsBase);
    static constexpr id_t POPUP_GROUP_ID = Ui::PubSub::sourceId(Ui::PubSub::SourceId::PopupBase);

    // id_t m_nextWsId    = WS_GROUP_ID;
    id_t m_nextPopupId = POPUP_GROUP_ID;

    // Event dispatch state
    Event                      m_currentEvent {};
    Ui::Render::click_result_t m_lastClickResult {};
    bool                       m_popupEventConsumed = false;

    static Ui::Res::Type::border_t clampRadii(const Ui::Res::Type::border_t & radii, fpx_t w, fpx_t h)
    {
        const fpx_t maxR = std::min(w, h) / 2;
        return { std::min(maxR, std::round(radii.topLeft)),
                 std::min(maxR, std::round(radii.topRight)),
                 std::min(maxR, std::round(radii.bottomRight)),
                 std::min(maxR, std::round(radii.bottomLeft)) };
    }

public:
    static constexpr id_t APP_SUBSCRIBER_ID = Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App);

    explicit WindowManager(const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
    {
    }

    ~WindowManager() override { shutdown(); }

    Ui::PubSub::Subscribe &   subscribe() { return m_subscribe; }
    Ui::Window::RenderQueue & renderQueue() { return m_renderQueue; }

    void drainDeferred() { m_subscribe.drainDeferred(); }

    // Host callback fired when a dialog closes (the host reads lastDialogAction/Data).
    void setOnDialogClose(Ui::task_fn_t callback) { m_onDialogClose = std::move(callback); }

    void setDialogContentResolver(std::function<std::wstring(const std::string &)> resolver)
    {
        m_dialogContentResolver = std::move(resolver);
    }

    void requestRender(id_t windowId) { m_renderQueue.request(windowId); }
    void requestMainRender() { m_renderQueue.request(MAIN_WINDOW_ID); }

    void requestContentRefresh()
    {
        m_contentIsDirty = true;
        requestMainRender();
    }

    // Double-click thresholds (host supplies these from its viewport config).
    void setDoubleClickConfig(uint32_t intervalMs, int distancePx)
    {
        if (m_eventHandler) {
            m_eventHandler->setDoubleClickConfig(intervalMs, distancePx);
        }
    }

    // The rect left for embedded content after the UI chrome (toolbars + docks
    // + menu/tab/status). Content surfaces are sized/positioned to this.
    [[nodiscard]] Ui::Res::Type::bound_t viewportBound() const
    {
        return { m_uiLeft,
                 m_uiTop,
                 std::max(1.0F, m_windowWidth - m_uiLeft - m_uiRight),
                 std::max(1.0F, m_windowHeight - m_uiTop - m_uiBottom) };
    }

    // ---- Content-surface registry (the host-facing seam) ----

    // Register a host-owned content surface. The framework wires its render
    // request to the queue, registers it with the event handler, and positions
    // it in the viewport. window/renderer must outlive the registration.
    void addContentSurface(id_t id, Ui::IWindow & window, Ui::IRenderer & renderer)
    {
        content_surface_t & entry = m_contentSurfaces[id];
        entry.window              = &window;
        entry.renderer            = &renderer;

        const id_t renderId = WS_GROUP_ID + id;
        window.setRenderRequest([this, renderId]() { m_renderQueue.request(renderId); });

        // Under Wayland the content window is a separate X11 window driven via
        // X11, not the main event loop, so it isn't registered here.
#ifndef HAVE_WAYLAND
        if (m_eventHandler) {
            m_eventHandler->registerChildWindow(id, window.nativeHandle());
        }
#endif

        positionContentSurface(window);
    }

    void removeContentSurface(id_t id)
    {
        auto it = m_contentSurfaces.find(id);
        if (it == m_contentSurfaces.end()) {
            return;
        }
#ifndef HAVE_WAYLAND
        if (m_eventHandler && it->second.window != nullptr) {
            m_eventHandler->unregisterChildWindow(it->second.window->nativeHandle());
        }
#endif
        // Composite texture lives in the main GL context; delete it there.
        if (it->second.composite.texture != 0 && m_mainWindow) {
            m_mainWindow->makeCurrent();
            it->second.composite.destroy();
        }
        m_contentSurfaces.erase(it);
        if (m_activeContent == id) {
            m_activeContent = Ui::INVALID_ID;
        }
    }

    // Make one content surface active: only it is shown (the windows overlap).
    void setActiveContentSurface(id_t id)
    {
        m_activeContent = id;
        for (auto & [surfaceId, entry] : m_contentSurfaces) {
            if (entry.window == nullptr) {
                continue;
            }
            if (g_config.isCompositing) {
                // Composite mode: windows stay offscreen; just repaint the active one.
                if (surfaceId == id) {
                    entry.window->requestRender();
                }
            } else if (surfaceId == id) {
                entry.window->show();
                // show() alone doesn't repaint the viewport; queue a frame.
                entry.window->requestRender();
            } else {
                entry.window->hide();
            }
        }
    }

    // Mark a content surface ready (or not) for rendering. The host clears this
    // while a model loads asynchronously so the framework's render loop skips
    // the surface (avoids empty-scene frames), and sets it once the load lands.
    void setContentSurfaceReady(id_t id, bool isReady)
    {
        auto it = m_contentSurfaces.find(id);
        if (it != m_contentSurfaces.end()) {
            it->second.isReady = isReady;
        }
    }

    // ---- Content-surface internal helpers (framework-side) ----

    [[nodiscard]] Ui::IWindow * contentWindow(id_t id) const
    {
        auto it = m_contentSurfaces.find(id);
        return (it != m_contentSurfaces.end()) ? it->second.window : nullptr;
    }

    [[nodiscard]] Ui::IWindow * activeContentWindow() const { return contentWindow(m_activeContent); }

    // Hit-test the active content surface, returning child-local coords on hit
    // (see ContentHit). Single lookup so the window pointer and its bound match.
    [[nodiscard]] ContentHit hitTestActiveContent(int x, int y) const
    {
        if (m_activeContent == Ui::INVALID_ID) {
            return {};
        }
        auto it = m_contentSurfaces.find(m_activeContent);
        if (it == m_contentSurfaces.end() || it->second.window == nullptr) {
            return {};
        }
        const Ui::Res::Type::bound_t b = it->second.window->bound();
        if (!b.contains(x, y)) {
            return {};
        }
        return { it->second.window, x - static_cast<int>(b.x), y - static_cast<int>(b.y) };
    }

    // Read the active content surface's framebuffer into its composite cache
    // (Wayland composite path). Generic raw read of the just-rendered surface -
    // bottom-up, matching the FlipY draw in blitChildTexture.
    static void captureContentComposite(content_surface_t & entry)
    {
        if (entry.window == nullptr) {
            return;
        }
        const int w = static_cast<int>(entry.window->bound().w);
        const int h = static_cast<int>(entry.window->bound().h);
        if (w <= 0 || h <= 0) {
            return;
        }
        entry.window->makeCurrent();
        entry.composite.pixels.resize(static_cast<size_t>(w) * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, entry.composite.pixels.data());
        entry.composite.w     = w;
        entry.composite.h     = h;
        entry.composite.dirty = true;
    }

    // Place a content surface at the current viewport rect. On Wayland the X11
    // child is parked offscreen (avoids surface flicker) but keeps its logical
    // position for hit testing.
    void positionContentSurface(Ui::IWindow & window) const
    {
        const Ui::Res::Type::bound_t vp = viewportBound();
        if (g_config.isCompositing) {
            window.moveResize({ -vp.w, -vp.h, vp.w, vp.h });
            window.setPosition(vp.x, vp.y);
        } else {
            window.moveResize(vp);
        }
    }

    [[nodiscard]] const Event &                      currentEvent() const { return m_currentEvent; }
    [[nodiscard]] const Ui::Render::click_result_t & lastClickResult() const { return m_lastClickResult; }
    [[nodiscard]] bool                               popupEventConsumed() const { return m_popupEventConsumed; }

    /**
     * @brief Check if a point (popup-local coords) is inside popup's visual region
     */
    [[nodiscard]] bool popupContainsPoint(int x, int y) const
    {
        return m_popupWindow && m_popupWindow->containsPoint(x, y);
    }

    /**
     * @brief Translate popup-local coords to main window coords
     */
    void popupToMainCoords(int popupX, int popupY, int & mainX, int & mainY) const
    {
        if (m_popupWindow) {
            mainX = static_cast<int>(m_popupWindow->bound().x) + popupX;
            mainY = static_cast<int>(m_popupWindow->bound().y) + popupY;
        } else {
            mainX = popupX;
            mainY = popupY;
        }
    }

    /**
     * @brief Dispatch a platform event: store, route to windows, notify subscribers
     *
     * Handles:
     * 1. Composite mode popup coordinate rerouting
     * 2. Mechanical routing of events to popup / main / child windows
     * 3. Notifying subscribers via Ui::PubSub::eventSourceId(EventType)
     *
     * The shell subscribes to event notifications for policy decisions (menu
     * management, shortcuts, popup close logic).
     */
    void dispatchEvent(Event event)
    {
        m_lastClickResult    = {};
        m_popupEventConsumed = false;

        // Classify event source window.
        // In composite mode: popups are offscreen, classify by bounds.
        // In non-composite mode: classify by native window handle.
        Ui::Window::Popup::PopupWindow * popupTarget = classifyPopupEvent(event);

        m_currentEvent = event;

        // Dialog is modal - only route events to the dialog window, block everything else
        if (m_dialogWindow) {
            if (popupTarget == m_dialogWindow.get()) {
                routePopupEvent(event, *m_dialogWindow);
            }
            m_subscribe.notify(Ui::PubSub::eventSourceId(event.type));
            return;
        }

        const bool isChildEvent = (event.childWindowId != Ui::INVALID_ID);
        const bool isPopupEvent = event.isPopupEvent;

        // Route popup-type events (dialog, submenu, popup menu)
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
            case EventType::MouseButtonPress:
                if (event.mouse.button == MouseButton::Left) {
                    if (isChildEvent) {
                        onMousePress(event.mouse.x, event.mouse.y, event.childWindowId, event.mouse.clickCount);
                    } else {
                        onMousePress(event.mouse.x, event.mouse.y, event.mouse.clickCount);
                    }
                }
                break;
            case EventType::MouseButtonRelease:
                if (event.mouse.button == MouseButton::Left) {
                    m_lastClickResult = onMouseRelease(event.mouse.x,
                                                       event.mouse.y,
                                                       isChildEvent ? event.childWindowId : Ui::INVALID_ID);
                }
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
        m_subscribe.notify(Ui::PubSub::eventSourceId(event.type));
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
        // getenv is safe here: single-threaded init and the process never setenv's.
        // NOLINTNEXTLINE(concurrency-mt-unsafe)
        g_config.isCompositing = (std::getenv("WAYLAND_DISPLAY") != nullptr);
#endif

        const auto & layout = m_resManager.layout();
        m_windowWidth       = layout.windowWidth;
        m_windowHeight      = layout.windowHeight;

        // Restore previously persisted window size if session.json carries it.
        // sessionWindowWidth/Height default to 0 when no prior run committed
        // geometry, in which case the layout defaults above are used as-is.
        const int sessionW = m_resManager.sessionWindowWidth();
        const int sessionH = m_resManager.sessionWindowHeight();
        if (sessionW > 0 && sessionH > 0) {
            m_windowWidth  = static_cast<fpx_t>(sessionW);
            m_windowHeight = static_cast<fpx_t>(sessionH);
        }

        // Create main window
        m_mainWindow = std::make_unique<NativeWindow>(m_subscribe, MAIN_WINDOW_ID);

        // Set background color before create() so window uses correct bg pixel
        m_mainWindow->setBackground(m_resManager.theme().second.bg);

        if (!m_mainWindow->create(m_windowWidth, m_windowHeight, nullptr, 0, m_resManager.title())) {
            std::cerr << "[WindowManager] Failed to create main window" << std::endl;
            return false;
        }

        // Update dimensions from actual window
        m_windowWidth  = m_mainWindow->bound().w;
        m_windowHeight = m_mainWindow->bound().h;

        // Restore window position. create() doesn't accept x/y, so we apply
        // the saved position after mapping via moveResize. May cause a brief
        // flash from the WM's default position to the restored one; the
        // alternative (XMoveWindow before XMapWindow) requires plumbing x/y
        // through create() on every platform. No-op on Wayland - clients
        // can't position themselves there. Width/height in the moveResize
        // are the just-validated bound() values so we don't fight the WM
        // on its sizing decision.
        if (sessionW > 0 && sessionH > 0) {
            m_mainWindow->moveResize({ static_cast<fpx_t>(m_resManager.sessionWindowX()),
                                       static_cast<fpx_t>(m_resManager.sessionWindowY()),
                                       m_windowWidth,
                                       m_windowHeight });
        }

        // Wire render request to Subscribe render queue
        m_mainWindow->setRenderRequest([this]() { m_renderQueue.request(MAIN_WINDOW_ID); });

        applyWindowIcon();

        m_mainWindow->makeCurrent();

#ifndef __APPLE__
        // Initialize GLEW (experimental mode required for core profile contexts)
        // macOS provides GL functions directly via framework - no extension loader needed.
        glewExperimental     = GL_TRUE;
        const GLenum glewErr = glewInit();
        if (glewErr != GLEW_OK) {
            // With core profile, glewInit may return GLEW_ERROR_NO_GLX_DISPLAY
            // This is actually OK for EGL - functions are still loaded
            if (glewErr == 4) { // GLEW_ERROR_NO_GLX_DISPLAY = 4
                std::cout << "[WindowManager] GLEW returned NO_GLX_DISPLAY (expected with EGL)" << std::endl;
            } else {
                std::cerr << "[WindowManager] GLEW init failed: " << glewGetErrorString(glewErr) << std::endl;
                return false;
            }
        }
        // Clear any GL error generated by glewInit on core profiles
        while (glGetError() != GL_NO_ERROR) { }
        std::cout << "[WindowManager] GLEW initialized successfully" << std::endl;
#endif

        // Instantiate one Ui::Render::DockColumn per parsed dock config. Keys are stable
        // (DockBase + index in the parsed array) so any Subscribe wiring that
        // happens later can address a dock by id. Initial state is pulled
        // from the session via ResManager::dockState(name) inside the ctor.
        m_docks.clear();
        const auto & dockConfigs = m_resManager.layout().docks;
        for (size_t i = 0; i < dockConfigs.size(); ++i) {
            const id_t id = Ui::PubSub::dockSourceId(i);
            m_docks.try_emplace(id, id, dockConfigs[i], m_resManager);
        }

        // Session-restored widths might overflow the current window (smaller
        // screen than last run, or hand-edited session.json). Clamp each
        // committed width against viewport availability now, before the
        // first layout/render pass, using the same maxDockContentWidth path
        // that drag-time and double-click-restore reuse.
        for (auto & [id, dock] : m_docks) {
            clampDockStateToViewport(dock);
        }

        // Preload the grip glyph so Ui::Render::DockColumn::render's first-frame size
        // query against SvgRenderer's cache hits rather than falling back to
        // a square. Shared across all docks; configured via --dock-grip-icon
        // in layout.json.
        if (!m_docks.empty()) {
            const std::string & iconName = m_resManager.layout().dockDefaults.gripIcon;
            if (!iconName.empty()) {
                Ui::Gl::SvgRenderer::loadFilledFromFile(m_resManager.resPath().icon(iconName));
            }
        }

        refreshDisplayMetrics();

        // Initialize platform-specific event handler. Double-click thresholds
        // come from the host via setDoubleClickConfig() after initialize().
        m_eventHandler = EventFactory::create();
        m_eventHandler->init(*m_mainWindow);

        std::cout << "[WindowManager] Main window initialized: " << m_mainWindow->nativeHandle() << " " << m_windowWidth
                  << "x" << m_windowHeight << std::endl;

        m_running = true;
        return true;
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
        if (!m_mainWindow) {
            return false;
        }
        const float prevScale = g_config.scale;
        g_config.dpi          = NativeWindow::queryDpi(m_mainWindow->nativeDisplay());
        g_config.scale        = static_cast<float>(g_config.dpi) / 96.0F;

        const auto & layout = m_resManager.layout();
        m_uiLeft            = toPhys(layout.leftToolbar.width);
        m_uiTop             = toPhys(layout.topMenu.height) + toPhys(layout.workspaceTab.height);
        m_uiRight           = toPhys(layout.rightToolbar.width);
        m_uiBottom          = toPhys(layout.statusBar.height);

        // Dock widths add on top of the toolbar margins; layoutDocks places
        // each dock's outer rect using the now-final viewport edges.
        applyDockMargins();
        layoutDocks();

        const bool scaleChanged = (prevScale != g_config.scale);
        std::cout << "[WindowManager] Scale factor: " << g_config.scale << "x  UI margins: L=" << m_uiLeft
                  << " T=" << m_uiTop << " R=" << m_uiRight << " B=" << m_uiBottom << " docks=" << m_docks.size()
                  << (scaleChanged ? " (changed)" : "") << std::endl;
        return scaleChanged;
    }

    // Add the current sum of dock widths into m_uiLeft / m_uiRight on top
    // of the toolbar widths. Called from refreshDisplayMetrics (which set
    // the toolbar baselines first) and from onDockStateCommitted (which
    // resets to toolbar widths then re-applies).
    void applyDockMargins()
    {
        for (const auto & [id, dock] : m_docks) {
            const fpx_t w = toPhys(dock.currentWidth());
            if (dock.anchor() == Ui::Res::Dock::DockAnchor::Left) {
                m_uiLeft += w;
            } else {
                m_uiRight += w;
            }
        }
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
        if (!m_mainWindow) {
            return;
        }
        const auto & layout = m_resManager.layout();
        m_uiLeft            = toPhys(layout.leftToolbar.width);
        m_uiRight           = toPhys(layout.rightToolbar.width);
        applyDockMargins();
        layoutDocks();

        const Ui::Res::Type::bound_t newViewport = viewportBound();
        // positionContentSurface() does the moveResize (+ Wayland offscreen fork)
        // that atomically updates position, size, and the cached bound() event
        // routing relies on. Then push the new size to each content renderer.
        for (auto & [id, entry] : m_contentSurfaces) {
            if (entry.window == nullptr) {
                continue;
            }
            positionContentSurface(*entry.window);
            if (entry.renderer != nullptr) {
                entry.renderer->resize(newViewport.w, newViewport.h);
            }
        }
        m_renderQueue.request(MAIN_WINDOW_ID);
    }

    // Maximum content width this dock can take right now without pushing
    // the content surface below zero: workspace - other docks' outer rects -
    // this dock's grip. Used by both the drag-time and the double-click
    // restore clamps; they differ only in which width they cap.
    [[nodiscard]] fpx_t maxDockContentWidth(const Ui::Render::DockColumn & dock) const
    {
        const auto & layout    = m_resManager.layout();
        const fpx_t  windowCss = toCss(m_windowWidth);
        fpx_t        othersSum = 0.0F;
        for (const auto & [id, other] : m_docks) {
            if (&other == &dock) {
                continue;
            }
            othersSum += other.currentWidth();
        }
        const fpx_t available = windowCss - layout.leftToolbar.width - layout.rightToolbar.width - othersSum;
        return std::max(0.0F, available - layout.dockDefaults.gripWidth);
    }

    // Clamp the dragging dock's transient width to what the viewport can
    // actually fit right now. Caller has already updated the dock's
    // transient via its onMouseMove; this is the second pass that enforces
    // the dynamic ceiling before reflow.
    void clampDraggingDockToViewport(Ui::Render::DockColumn & dock) const
    {
        dock.clampTransientWidth(maxDockContentWidth(dock));
    }

    // Same ceiling, but applied to the committed (session-persisted) width.
    // Called after a non-drag state change (double-click restore) so the
    // freshly-revealed dock can't overlap others or push content surface below zero.
    void clampDockStateToViewport(Ui::Render::DockColumn & dock) const
    {
        dock.clampStateWidth(maxDockContentWidth(dock));
    }

    // Position each dock's outer rect. Each dock is anchored to its toolbar-
    // facing edge (away from the viewport): a left dock's left edge sits
    // against the left toolbar or the next outer dock; growing it pushes the
    // right (viewport-facing) edge inward. Symmetric on the right side.
    //
    // That anchoring is what makes drag-to-resize feel right: during a drag
    // only the dragging dock's currentWidth() changes, but the toolbar-facing
    // edge stays put, so the gesture-followed edge is the grip itself rather
    // than some far edge.
    //
    // Walk order DESC so the outermost dock is positioned first against the
    // toolbar, then each inner dock packs against the previous. CSS units
    // throughout - Ui::Render::UiRenderer's draw ops are CSS-native.
    void layoutDocks()
    {
        if (m_docks.empty()) {
            return;
        }
        const auto & layout          = m_resManager.layout();
        const fpx_t  bodyTop         = toCss(m_uiTop);
        const fpx_t  bodyH           = std::max(1.0F, toCss(m_windowHeight - m_uiTop - m_uiBottom));
        const fpx_t  leftToolbarCss  = layout.leftToolbar.width;
        const fpx_t  rightToolbarCss = layout.rightToolbar.width;
        const fpx_t  windowWidthCss  = toCss(m_windowWidth);

        std::vector<Ui::Render::DockColumn *> left;
        std::vector<Ui::Render::DockColumn *> right;
        for (auto & [id, dock] : m_docks) {
            if (dock.anchor() == Ui::Res::Dock::DockAnchor::Left) {
                left.emplace_back(&dock);
            } else {
                right.emplace_back(&dock);
            }
        }
        // DESC: highest order first (outermost), packs inward as we walk.
        auto byOrderDesc = [](const Ui::Render::DockColumn * a, const Ui::Render::DockColumn * b) {
            return a->order() > b->order();
        };
        std::sort(left.begin(), left.end(), byOrderDesc);
        std::sort(right.begin(), right.end(), byOrderDesc);

        // Left docks: pack rightward from the left toolbar's inner edge.
        // innerEdgeX for a Left dock is its viewport-facing (right) edge.
        fpx_t leftEdge = leftToolbarCss;
        for (auto * d : left) {
            const fpx_t rightEdgeOfDock = leftEdge + d->currentWidth();
            d->setLayout(bodyTop, bodyH, rightEdgeOfDock);
            leftEdge = rightEdgeOfDock;
        }

        // Right docks: pack leftward from the right toolbar's inner edge.
        // innerEdgeX for a Right dock is its viewport-facing (left) edge.
        fpx_t rightEdge = windowWidthCss - rightToolbarCss;
        for (auto * d : right) {
            const fpx_t leftEdgeOfDock = rightEdge - d->currentWidth();
            d->setLayout(bodyTop, bodyH, leftEdgeOfDock);
            rightEdge = leftEdgeOfDock;
        }
    }

    // Set the main-window icon from icon-defaults.json roles. The symbolic icon
    // is optional - pass an empty path so the platform layer skips it rather than
    // trying to load the icon directory as an SVG.
    void applyWindowIcon()
    {
        if (!m_mainWindow) {
            return;
        }
        const std::string mainIcon = m_resManager.iconDefault("window-icon").icon;
        if (mainIcon.empty()) {
            return;
        }
        const std::string symbolicIcon = m_resManager.iconDefault("window-icon-symbolic").icon;
        const auto &      resPath      = m_resManager.resPath();
        m_mainWindow->setWindowIcon(resPath.icon(mainIcon),
                                    symbolicIcon.empty() ? std::string {} : resPath.icon(symbolicIcon));
    }

    /**
     * @brief Apply resource changes to all windows and renderers
     */
    void apply(Ui::Res::Type::Changed changed)
    {
        // Layout reload may have added, removed, or renamed dock configs.
        // Wipe-and-rebuild matches initialize() exactly; persisted widths
        // survive because Ui::Render::DockColumn's ctor pulls them from ResManager's
        // by-name dock-state map. Drag capture is cleared so a stale id
        // (dock removed mid-drag) can't route into a missing entry.
        if (Common::Bit::And(changed, Ui::Res::Type::Changed::Layout) != 0) {
            m_activeDragDock = Ui::INVALID_ID;
            m_docks.clear();
            const auto & dockConfigs = m_resManager.layout().docks;
            for (size_t i = 0; i < dockConfigs.size(); ++i) {
                const id_t id = Ui::PubSub::dockSourceId(i);
                m_docks.try_emplace(id, id, dockConfigs[i], m_resManager);
            }
            for (auto & [id, dock] : m_docks) {
                clampDockStateToViewport(dock);
            }
            if (!m_docks.empty()) {
                const std::string & iconName = m_resManager.layout().dockDefaults.gripIcon;
                if (!iconName.empty()) {
                    Ui::Gl::SvgRenderer::loadFilledFromFile(m_resManager.resPath().icon(iconName));
                }
            }
            // Recompute toolbar+dock margins, place each dock, push the new
            // viewport to content surfaces, and queue a main-window repaint.
            reflowDocksAndViewport();
        }

        // Window icons live in icon-defaults.json; re-read them when that file changed.
        if (Common::Bit::And(changed, Ui::Res::Type::Changed::Icon) != 0) {
            applyWindowIcon();
        }

        // Main window: setBackground() calls makeCurrent() internally
        if (m_mainWindow && Common::Bit::And(changed, Ui::Res::Type::Changed::Theme) != 0) {
            m_mainWindow->setBackground(m_resManager.theme().second.bg);
        }

        if (m_mainWindow && m_mainWindow->hasRenderer()) {
            m_mainWindow->activeRenderer().apply(changed);
            m_mainWindow->requestRender();
        }

        // Content surfaces: each setBackground() makes its own context current.
        for (auto & [id, entry] : m_contentSurfaces) {
            if (entry.window == nullptr) {
                continue;
            }
            if (Common::Bit::And(changed, Ui::Res::Type::Changed::Theme) != 0) {
                entry.window->setBackground(m_resManager.theme().workspace.bg);
            }
            if (entry.renderer != nullptr) {
                entry.renderer->apply(changed);
                entry.window->requestRender();
            }
        }

        // Restore main window context (a content setBackground may have switched it)
        if (m_mainWindow) {
            m_mainWindow->makeCurrent();
        }
    }

    /**
     * @brief Get popup window (or nullptr if not created)
     */
    Ui::Window::Popup::PopupWindow * popupWindow() { return m_popupWindow.get(); }

    // True when the open popup is an X11 child of the main window and so tracks
    // the parent's move/resize itself - the shell must not move/recreate it.
    [[nodiscard]] bool popupFollowsParent() const { return m_popupWindow && m_popupWindow->followsParent(); }

    /**
     * @brief Create popup window at screen coordinates with uniform corner radius
     * @param screenX Absolute X coordinate
     * @param screenY Absolute Y coordinate
     * @param width Popup width
     * @param height Popup height
     * @param cornerRadius Corner radius for software blending (used when true alpha unavailable)
     * @return true on success
     */
    /**
     * @brief Create popup window at screen coordinates with per-corner radii
     * @param bound Screen position (x, y) and size (w, h)
     * @param radii Per-corner radii (topLeft, topRight, bottomRight, bottomLeft)
     * @param bgColor Background color
     * @param parentPixels Pre-captured parent framebuffer for corner blending
     * @param parentPixelW Parent framebuffer width
     * @param parentPixelH Parent framebuffer height
     * @return true on success
     */
    bool createPopup(const Ui::Res::Type::bound_t &  bound,
                     const Ui::Res::Type::border_t & radii,
                     const Ui::Color &               bgColor)
    {
        if (m_popupWindow) {
            // Sync display to ensure old popup is removed before capturing corners for new one
            destroyPopup(true);
        }

        m_popupWindow = std::make_unique<Ui::Window::Popup::PopupWindow>(m_subscribe, ++m_nextPopupId);

        // Convert absolute screen coords to coordinates relative to main window
        int parentScreenX {};
        int parentScreenY {};
        mainWindowScreenPosition(parentScreenX, parentScreenY);
        const fpx_t relX = bound.x - parentScreenX;
        const fpx_t relY = bound.y - parentScreenY;

        // Set position and background before create
        m_popupWindow->setPosition(relX, relY);
        m_popupWindow->setBackground(bgColor);

        // Set per-corner radii (values < 0.5 treated as zero)
        if (radii.anyNonZero()) {
            m_popupWindow->setCornerRadii(radii);
        }

        if (!m_popupWindow->create(*m_mainWindow, bound.w, bound.h)) {
            std::cerr << "[WindowManager] Failed to create popup window (screen=" << bound.x << "," << bound.y
                      << ", rel=" << relX << "," << relY << ")" << std::endl;
            m_popupWindow.reset();
            return false;
        }

        // Register popup with event handler
        if (m_eventHandler) {
            m_eventHandler->addPopupWindow(m_popupWindow->nativeHandle());
        }

        // On Wayland, move popup offscreen to prevent XWayland surface flicker.
        // Keep it mapped for valid EGL rendering.
        if (g_config.isCompositing) {
            m_popupWindow->move(-bound.w, -bound.h);
            m_popupWindow->setPosition(relX, relY);
        }

        // Wire popup render request to Subscribe render queue
        m_popupWindow->setRenderRequest([this]() { m_renderQueue.request(m_popupWindow->subscribeId()); });

        // Subscribe popup to content changes from main window + content surfaces
        const id_t popupSubId = m_popupWindow->subscribeId();
        m_subscribe.add(MAIN_WINDOW_ID, popupSubId, [this]() { recapturePopupCorners(); });
        for (const auto & [id, entry] : m_contentSurfaces) {
            m_subscribe.add(WS_GROUP_ID + id, popupSubId, [this]() { recapturePopupCorners(); });
        }

        std::cout << "[WindowManager] Created popup at screen(" << bound.x << "," << bound.y << ") rel(" << relX << ","
                  << relY << ") size " << bound.w << "x" << bound.h << std::endl;
        return true;
    }

    /**
     * @brief Initialize popup renderer and capture initial corner pixels.
     *
     * Must be called after createPopup(). Renders main window to back buffer,
     * captures corners, initializes the popup renderer with corner data,
     * then renders and shows the first popup frame.
     */
    void initPopupRenderer()
    {
        if (!m_popupWindow) {
            return;
        }

        // Find active menu
        const auto & menus  = m_resManager.menus();
        const id_t   menuId = m_resManager.activeMenuId();
        auto         it = std::find_if(menus.begin(), menus.end(), [menuId](const auto & m) { return m.id == menuId; });
        if (it == menus.end()) {
            std::cerr << "[WindowManager] Active menu not found: " << menuId << std::endl;
            return;
        }

        // Render main window to get fresh back buffer for corner capture
        renderMainWindow(true);

        m_popupWindow->initRenderer(m_resManager, *it);

        // Wire submenu hover callback
        auto * popupRenderer = (m_popupWindow->hasRenderer() ? &m_popupWindow->menuRenderer() : nullptr);
        if (popupRenderer != nullptr) {
            popupRenderer->setSubmenuHoverCallback(
            [this](const Ui::Res::Type::bound_t & itemBound,
                   const Ui::Res::Type::menu_t &  item,
                   bool                           isFirst,
                   bool                           isLast) { onSubmenuHover(itemBound, item, isFirst, isLast); });
        }

        // Initial corner capture
        recapturePopupCorners();

        swapMainWindow();
    }

    /**
     * @brief Destroy submenu popup window
     */
    void destroySubmenu()
    {
        // Clear submenu parent highlight from parent popup renderer
        updateSubmenuParentHighlight(Ui::INVALID_ID);

        if (m_submenuWindow) {
            m_submenuWindow.reset();
            m_submenuComposite.clear();
            // m_submenuItemId is the *caller's* state. Resetting it here would
            // overwrite the value the caller set when switching to a new
            // submenu (createSubmenu calls us mid-transition with the new id
            // already in m_submenuItemId).

            // Restore parent popup's TR and BR corners
            if (m_popupWindow && m_mainWindow) {
                const Ui::Res::Type::border_t originalRadii = m_resManager.layout().topMenuDropdown.border.scaled(
                g_config.scale);
                Ui::Res::Type::border_t parentRadii = m_popupWindow->cornerRadii();
                parentRadii.topRight                = originalRadii.topRight;
                parentRadii.bottomRight             = originalRadii.bottomRight;
                m_popupWindow->setCornerRadii(parentRadii);

                auto * popupRenderer = (m_popupWindow->hasRenderer() ? &m_popupWindow->menuRenderer() : nullptr);
                if (popupRenderer != nullptr) {
                    popupRenderer->setContainerBorder(m_resManager.layout().topMenuDropdown.border);
                }

                // Render main window for fresh back buffer, then recapture BR corner
                renderMainWindow(true);
                recaptureCorners(*m_popupWindow, m_popupComposite);
                drawPopupComposite();
                swapMainWindow();
            }

            if (m_mainWindow) {
                m_mainWindow->makeCurrent();
            }
            std::cout << "[WindowManager] Destroyed submenu window" << std::endl;
        }
    }

    [[nodiscard]] bool hasSubmenu() const { return m_submenuWindow && m_submenuWindow->isValid(); }

    [[nodiscard]] id_t submenuItemId() const { return m_submenuItemId; }

    // Re-open a submenu for the given parent item id, if a popup is currently
    // shown and contains that item. Used to restore submenu state after a
    // resource reload, where the popup was destroyed and recreated.
    void reopenSubmenu(id_t parentItemId)
    {
        if (parentItemId == Ui::INVALID_ID || !m_popupWindow) {
            return;
        }
        auto * popupRenderer = (m_popupWindow->hasRenderer() ? &m_popupWindow->menuRenderer() : nullptr);
        if (popupRenderer == nullptr) {
            return;
        }
        const Ui::Render::UiElement * el = popupRenderer->findElementById(parentItemId);
        if (el == nullptr) {
            return;
        }
        const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(parentItemId);
        if (item.items.empty()) {
            return;
        }
        const Ui::Res::Type::bound_t physBound = { toPhysRound(el->bound.x),
                                                   toPhysRound(el->bound.y),
                                                   toPhysRound(el->bound.w),
                                                   toPhysRound(el->bound.h) };
        const bool                   isFirst   = popupRenderer->isFirstElement(parentItemId);
        const bool                   isLast    = popupRenderer->isLastElement(parentItemId);
        onSubmenuHover(physBound, item, isFirst, isLast);
    }

    Ui::Window::Popup::PopupWindow * submenuWindow() { return m_submenuWindow.get(); }

    void updateSubmenuParentHighlight(id_t parentId)
    {
        if (!m_popupWindow) {
            return;
        }
        auto * popupRenderer = (m_popupWindow->hasRenderer() ? &m_popupWindow->menuRenderer() : nullptr);
        if (popupRenderer != nullptr) {
            popupRenderer->setSubmenuParentId(parentId);
            m_popupWindow->requestRender();
        }
    }

    // Highlighted item across the open popup + submenu (submenu wins, being the
    // deeper window). Saved/restored across a reload that recreates the windows.
    [[nodiscard]] id_t activePopupItemId() const
    {
        if (auto * renderer = submenuRenderer(); renderer != nullptr) {
            if (const id_t id = renderer->hoveredItemId(); id != Ui::INVALID_ID) {
                return id;
            }
        }
        if (auto * renderer = popupRenderer(); renderer != nullptr) {
            return renderer->hoveredItemId();
        }
        return Ui::INVALID_ID;
    }

    // Re-highlight an item on whichever of the popup/submenu actually holds it.
    void setActivePopupItem(id_t id)
    {
        if (auto * renderer = submenuRenderer(); renderer != nullptr && renderer->findElementById(id) != nullptr) {
            renderer->setHoveredItem(id);
            m_submenuWindow->requestRender();
            return;
        }
        if (auto * renderer = popupRenderer(); renderer != nullptr && renderer->findElementById(id) != nullptr) {
            renderer->setHoveredItem(id);
            m_popupWindow->requestRender();
        }
    }

    /**
     * @brief Create submenu popup at screen coordinates
     */
    bool createSubmenu(const Ui::Res::Type::bound_t &  bound,
                       const Ui::Res::Type::border_t & radii,
                       const Ui::Color &               bgColor)
    {
        destroySubmenu();

        const id_t submenuId = ++m_nextPopupId;
        m_submenuWindow      = std::make_unique<Ui::Window::Popup::PopupWindow>(m_subscribe, submenuId);

        int parentScreenX {};
        int parentScreenY {};
        mainWindowScreenPosition(parentScreenX, parentScreenY);
        const fpx_t relX = bound.x - parentScreenX;
        const fpx_t relY = bound.y - parentScreenY;

        m_submenuWindow->setPosition(relX, relY);
        m_submenuWindow->setBackground(bgColor);

        if (radii.anyNonZero()) {
            m_submenuWindow->setCornerRadii(radii);
        }

        if (!m_submenuWindow->create(*m_mainWindow, bound.w, bound.h)) {
            std::cerr << "[WindowManager] Failed to create submenu window" << std::endl;
            m_submenuWindow.reset();
            return false;
        }

        if (g_config.isCompositing) {
            m_submenuWindow->move(-bound.w, -bound.h);
            m_submenuWindow->setPosition(relX, relY);
        }

        m_submenuWindow->setRenderRequest([this]() { m_renderQueue.request(m_submenuWindow->subscribeId()); });

        // Subscribe submenu to content changes from main window + content surfaces
        // (same pattern as popup - keeps corner capture fresh when a content
        // surface redraws while the submenu is open).
        m_subscribe.add(MAIN_WINDOW_ID, submenuId, [this]() { recaptureSubmenuCorners(); });
        for (const auto & [id, entry] : m_contentSurfaces) {
            m_subscribe.add(WS_GROUP_ID + id, submenuId, [this]() { recaptureSubmenuCorners(); });
        }

        std::cout << "[WindowManager] Created submenu at screen(" << bound.x << "," << bound.y << ") size " << bound.w
                  << "x" << bound.h << std::endl;
        return true;
    }

    /**
     * @brief Initialize submenu renderer and capture corner pixels
     */
    void initSubmenuRenderer(const Ui::Res::Type::menu_t & submenu)
    {
        if (!m_submenuWindow) {
            return;
        }

        // Render main window to back buffer for corner capture (do NOT swap -
        // swapping without popup composite would flash the popup away)
        renderMainWindow(true);
        m_submenuWindow->initRenderer(m_resManager, submenu);
        recaptureCorners(*m_submenuWindow, m_submenuComposite);

        // Redraw popup composite on main window before swap
        drawPopupComposite();
        swapMainWindow();
    }

    // ---- Dialog window ----

    void openDialog(const Ui::Res::Type::dialog_t & dialog)
    {
        closeDialog();

        m_lastDialogData = dialog;

        // Resolve content: file > locale with placeholders
        Ui::Res::Type::dialog_t resolved = dialog;
        if (!resolved.file.empty()) {
            std::ifstream file(resolved.file, std::ios::binary);
            if (file.is_open()) {
                const std::string  bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                const std::wstring content = Common::Unicode::fromUtf8(bytes);
                if (content.size() > Common::Sanitize::MAX_FILE_SIZE) {
                    std::cerr << "[Sanitize] File content truncated: " << resolved.file << std::endl;
                    resolved.contentOverride = content.substr(0, Common::Sanitize::MAX_FILE_SIZE);
                } else {
                    resolved.contentOverride = content;
                }
            }
        } else if (!resolved.content.empty()) {
            const std::string content = m_resManager.localeManager().get(resolved.content);
            resolved.contentOverride  = m_dialogContentResolver ? m_dialogContentResolver(content)
                                                                : Common::Unicode::fromUtf8(content);
        }

        const id_t dialogId = ++m_nextPopupId;
        m_dialogWindow      = std::make_unique<Ui::Window::Popup::DialogWindow>(m_subscribe, dialogId);

        m_dialogWindow->setRenderRequest([this]() { m_renderQueue.request(m_dialogWindow->subscribeId()); });

        if (!m_dialogWindow->open(*m_mainWindow,
                                  m_resManager,
                                  resolved,
                                  [this, resolved](Ui::Res::Type::DialogAction result) {
                                      m_lastDialogResult = result;
                                      m_lastDialogData   = resolved;
                                      m_subscribe.defer([this]() {
                                          if (m_onDialogClose) {
                                              m_onDialogClose();
                                          }
                                          closeDialog();
                                      });
                                  })) {
            m_dialogWindow.reset();
            return;
        }

        // Wire smooth scroll animation to render queue
        if (m_dialogWindow->hasRenderer()) {
            m_dialogWindow->dialogRenderer().setRenderRequest(
            [this]() { m_renderQueue.request(m_dialogWindow->subscribeId()); });
        }

        renderMainWindow(true);
        recaptureCorners(*m_dialogWindow, m_dialogComposite);
        drawPopupComposite();
        swapMainWindow();

        // Render dialog content into its back buffer BEFORE making the window visible;
        // otherwise an empty borderless NSWindow flashes for a frame (macOS orderFront is synchronous).
        if (!g_config.isCompositing && m_dialogWindow->render()) {
            m_dialogWindow->swapBuffers();
        }

        if (!g_config.isCompositing) {
            m_dialogWindow->show();
        }

        // Dialogs have no hover/mouse activity to piggy-back on after open, so the first
        // render+swap above can land on a drawable that is not yet attached to a visible
        // window (macOS NSOpenGLView - Platform::MacOsWindow::show binds the drawable on first show).
        // Queue one more frame so renderAll() repaints onto the now-bound surface.
        m_renderQueue.request(m_dialogWindow->subscribeId());

        m_mainWindow->makeCurrent();
    }

    void closeDialog()
    {
        if (m_dialogWindow) {
            m_dialogWindow.reset();
            m_dialogComposite.clear();

            if (m_mainWindow) {
                m_mainWindow->makeCurrent();
                renderMainAndSwap();
            }
            std::cout << "[WindowManager] Dialog closed" << std::endl;
        }
    }

    [[nodiscard]] bool                        hasDialog() const { return m_dialogWindow && m_dialogWindow->isValid(); }
    [[nodiscard]] Ui::Res::Type::DialogAction lastDialogAction() const { return m_lastDialogResult; }
    [[nodiscard]] const Ui::Res::Type::dialog_t & lastDialogData() const { return m_lastDialogData; }

    void confirmDialog()
    {
        if (!m_dialogWindow || !m_dialogWindow->hasRenderer()) {
            return;
        }
        m_dialogWindow->dialogRenderer().confirmPrimary();
    }

    void dismissDialog()
    {
        if (!m_dialogWindow || !m_dialogWindow->hasRenderer()) {
            return;
        }
        m_dialogWindow->dialogRenderer().dismiss();
    }

    void dialogKeyPress(const std::string & key)
    {
        if (!m_dialogWindow || !m_dialogWindow->hasRenderer()) {
            return;
        }
        m_dialogWindow->dialogRenderer().onKeyPress(key);
    }

    /**
     * @brief Handle submenu hover from PopupRenderer callback
     *
     * Opens a submenu to the right of the hovered item, or closes it
     * when hovering a non-submenu item (empty submenus).
     */
    void onSubmenuHover(const Ui::Res::Type::bound_t & itemBound,
                        const Ui::Res::Type::menu_t &  item,
                        bool                           isFirst,
                        bool                           isLast)
    {
        // Close submenu when hovering a non-submenu item
        if (item.items.empty()) {
            if (m_submenuItemId != Ui::INVALID_ID) {
                m_submenuItemId = Ui::INVALID_ID;
                destroySubmenu();
            }
            return;
        }

        // Skip if submenu is already open for this item
        if (item.id == m_submenuItemId && m_submenuWindow) {
            return;
        }
        m_submenuItemId = item.id;

        // Calculate submenu position: to the right of the hovered item
        const Ui::Res::Type::bound_t & popupBound = m_popupWindow->bound();

        int parentScreenX {};
        int parentScreenY {};
        mainWindowScreenPosition(parentScreenX, parentScreenY);

        const fpx_t submenuX = parentScreenX + popupBound.x + popupBound.w;
        const fpx_t submenuY = parentScreenY + popupBound.y + itemBound.y;

        // Calculate submenu size from items
        const auto & popup          = m_resManager.popup();
        const fpx_t  submenuCssW    = m_resManager.layout().topMenuDropdown.width;
        int          regularCount   = 0;
        int          separatorCount = 0;
        for (const auto & sub : item.items) {
            if (!sub.visible) {
                continue;
            }
            if (sub.separator) {
                ++separatorCount;
            } else {
                ++regularCount;
            }
        }
        const fpx_t contentCssH = regularCount * popup.itemHeight
                                + separatorCount * (popup.separatorHeight + popup.separatorMarginV * 2);
        // Round to integer physical px divisible by g_config.scale (see app.h createMenuPopup)
        const fpx_t submenuW = toPhysRound(submenuCssW);
        const fpx_t submenuH = toPhysRound(contentCssH);

        Ui::Res::Type::border_t submenuRadii = m_resManager.layout().topMenuDropdown.border.scaled(g_config.scale);

        // Submenu TL is always flat (connects to parent right edge)
        submenuRadii.topLeft = 0;

        if (isFirst || isLast) {
            Ui::Res::Type::border_t parentRadii = m_popupWindow->cornerRadii();
            Ui::Res::Type::border_t cssBorder   = m_resManager.layout().topMenuDropdown.border;

            auto * popupRenderer = (m_popupWindow->hasRenderer() ? &m_popupWindow->menuRenderer() : nullptr);

            if (isFirst) {
                parentRadii.topRight = 0;
                cssBorder.topRight   = 0;
            }
            if (isLast) {
                parentRadii.bottomRight = 0;
                cssBorder.bottomRight   = 0;
            }

            m_popupWindow->setCornerRadii(parentRadii);

            if (popupRenderer != nullptr) {
                popupRenderer->setContainerBorder(cssBorder);
                m_popupWindow->makeCurrent();
                if (isFirst) {
                    popupRenderer->updateCorner(1, {}, 0);
                }
                if (isLast) {
                    popupRenderer->updateCorner(2, {}, 0);
                }
                m_mainWindow->makeCurrent();
            }
            m_popupWindow->requestRender();
        }

        const Ui::Res::Type::bound_t submenuBound = { submenuX, submenuY, submenuW, submenuH };
        if (!createSubmenu(submenuBound, submenuRadii, m_resManager.theme().second.bg)) {
            return;
        }

        Ui::Res::Type::menu_t submenu;
        submenu.items       = item.items;
        submenu.popupHeight = contentCssH;
        initSubmenuRenderer(submenu);

        auto * submenuWin = submenuWindow();
        if (submenuWin == nullptr) {
            return;
        }

        if (submenuWin->render()) {
            if (g_config.isCompositing) {
                captureComposite(*submenuWin, m_submenuComposite);
                refreshComposite();
            } else {
                submenuWin->swapBuffers();
            }
        }

        if (!g_config.isCompositing) {
            submenuWin->show();
        }

        // Mark submenu parent in parent popup so it stays highlighted
        updateSubmenuParentHighlight(item.id);

        m_mainWindow->makeCurrent();
    }

    /**
     * @brief Destroy popup window
     * @param syncDisplay If true, sync with X server to ensure window is removed before returning
     */
    void destroyPopup(bool syncDisplay = false)
    {
        destroySubmenu();

        if (m_popupWindow) {
            // Unregister from event handler
            if (m_eventHandler) {
                m_eventHandler->removePopupWindow(m_popupWindow->nativeHandle());
            }
            // reset() triggers destructor which calls cleanupWayland() (xdg_popup,
            // xdg_surface) then Platform::WaylandWindow::destroy() (wl_surface, EGL).
            // Both must happen before the sync to avoid "invalid object" errors.
            m_popupWindow.reset();
            m_popupComposite.clear();

            // Sync to ensure the popup is removed from screen before we capture corners
            // for a new popup (otherwise we capture the old popup content)
            if (syncDisplay && m_mainWindow) {
#if defined(HAVE_X11) && !defined(HAVE_WAYLAND)
                auto * display = m_mainWindow->nativeDisplay(); // NOLINT(cppcoreguidelines-init-variables)
                if (display != nullptr) {
                    XSync(display, Ui::Window::Platform::X11::False);
                }
#elif defined(HAVE_WAYLAND)
                if (m_mainWindow->nativeDisplay() != nullptr) {
                    wl_display_roundtrip(m_mainWindow->nativeDisplay());
                }
#endif
            }

            // Restore main window context (popup destruction may have invalidated it)
            if (m_mainWindow) {
                m_mainWindow->makeCurrent();
            }

            std::cout << "[WindowManager] Destroyed popup window" << std::endl;
        }
    }

    /**
     * @brief Check if popup window exists
     */
    [[nodiscard]] bool hasPopup() const { return m_popupWindow && m_popupWindow->isValid(); }

    /**
     * @brief Capture popup pixels for composite mode (Wayland)
     * Call from app after popup renders (back buffer has content, no swap needed).
     */
    void capturePopupPixels()
    {
        if (m_popupWindow) {
            captureComposite(*m_popupWindow, m_popupComposite);
        }
    }

    static void captureComposite(const Ui::Window::Popup::PopupWindow & window, CompositeTexture & texture)
    {
        texture.capture(static_cast<int>(window.bound().w), static_cast<int>(window.bound().h));
    }

    /**
     * @brief Get the main window
     */
    [[nodiscard]] NativeWindow & mainWindow() { return *m_mainWindow; }

    /**
     * @brief Get main window's absolute screen position
     */
    void mainWindowScreenPosition(int & screenX, int & screenY) const
    {
        if (m_mainWindow) {
            m_mainWindow->screenPosition(screenX, screenY);
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
        if (m_mainWindow) {
            m_mainWindow->screenFramePosition(screenX, screenY);
        } else {
            screenX = 0;
            screenY = 0;
        }
    }

    [[nodiscard]] fpx_t windowWidth() const { return m_windowWidth; }
    [[nodiscard]] fpx_t windowHeight() const { return m_windowHeight; }

    // -------- Ui::Render::UiRenderer management --------

    /**
     * @brief Create Ui::Render::UiRenderer, initialize it, and transfer ownership to mainWindow
     */
    void initUiRenderer()
    {
        // The fw renderer never holds a window; give it a makeCurrent callback
        // over the main window plus the current physical size.
        auto uiRenderer = std::make_unique<Ui::Render::UiRenderer>(
        [window = m_mainWindow.get()] { window->makeCurrent(); },
        m_windowWidth,
        m_windowHeight,
        m_resManager);
        m_uiRenderer = uiRenderer.get();
        m_uiRenderer->setContent();
        m_uiRenderer->setExtraOpsHook([this](Ui::Render::UiRenderer & out) {
            for (const auto & [id, dock] : m_docks) {
                dock.render(out);
            }
        });
        m_mainWindow->setRenderer(std::move(uiRenderer));
    }

    /**
     * @brief Set hover callback on Ui::Render::UiRenderer
     */
    void setOnElementHover(std::function<void(Ui::Render::UiElementType, id_t)> cb)
    {
        if (m_uiRenderer != nullptr) {
            m_uiRenderer->setOnElementHover(std::move(cb));
        }
    }

    /**
     * @brief Get element bound from Ui::Render::UiRenderer by element id
     */
    [[nodiscard]] const Ui::Res::Type::bound_t & elementBound(id_t id) const
    {
        static const Ui::Res::Type::bound_t empty {};
        return m_uiRenderer != nullptr ? m_uiRenderer->bound(id) : empty;
    }

    /**
     * @brief Rebuild UI content (after data changes)
     */
    void setUIContent()
    {
        if (m_uiRenderer != nullptr) {
            m_uiRenderer->setContent();
        }
    }

    /**
     * @brief Resize the main window Ui::Render::UiRenderer
     */
    void resizeMainRenderer(fpx_t w, fpx_t h)
    {
        if (m_uiRenderer != nullptr) {
            m_uiRenderer->resize(w, h);
        }
    }

    /**
     * @brief Render main window UI frame and swap buffers
     * @param fullRedraw If true, clear framebuffer before rendering (needed for resize, theme switch, etc.)
     *                   If false, skip clear and just render + swap (UI elements draw opaque backgrounds)
     */
    /**
     * @brief Render main window content to back buffer (no swap).
     *
     * Call swapMainWindow() after to present. This split allows reading
     * from the back buffer (e.g. corner capture for popups) between render and swap.
     */
    void renderMainWindow(bool fullRedraw)
    {
        if (m_uiRenderer == nullptr || !m_mainWindow) {
            return;
        }
        m_mainWindow->makeCurrent();

        if (fullRedraw) {
            m_mainWindow->clear();
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
        m_uiRenderer->Render(m_windowWidth, m_windowHeight);

        // In composite mode, draw content surfaces as textures (no popup yet --
        // popup composite is drawn after corner capture to avoid ghost artifacts)
        if (g_config.isCompositing) {
            drawContentComposite();
        }
    }

    /**
     * @brief Draw popup composite texture on the main window back buffer.
     *
     * Call after notifyMainWindowChanged() and before swapMainWindow()
     * so corner capture reads UI + content surface only (no popup ghost).
     */
    void drawPopupComposite()
    {
        if (!g_config.isCompositing) {
            return;
        }
        if (m_popupWindow && !m_popupComposite.pixels.empty()) {
            drawCompositeTexture(m_popupComposite, m_popupWindow->bound());
        }
        if (m_submenuWindow && !m_submenuComposite.pixels.empty()) {
            drawCompositeTexture(m_submenuComposite, m_submenuWindow->bound());
        }
        if (m_dialogWindow && !m_dialogComposite.pixels.empty()) {
            drawCompositeTexture(m_dialogComposite, m_dialogWindow->bound());
        }
    }

    /**
     * @brief Notify content-changed subscribers.
     *
     * Call between renderMainWindow() and swapMainWindow().
     * Runs outside renderMainWindow to avoid GL context switches
     * during an active render (which corrupts GL state).
     */
    void notifyMainWindowChanged() { m_subscribe.notify(MAIN_WINDOW_ID); }

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
        if (const size_t pending = m_renderQueue.pending().size(); pending != 1) {
            std::cout << "[WM] renderAll pending=" << pending << std::endl;
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
            m_mainWindow->makeCurrent();
            setUIContent();
            m_contentIsDirty = false;
        }

        // 3. Drain render queue (main + popup)
        renderFrame();
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
    void renderFrame()
    {
        const auto pending = m_renderQueue.pending();
        if (pending.empty()) {
            return;
        }

        // Clear before rendering so requests added during rendering survive
        m_renderQueue.clear();

        const bool renderMain    = pending.contains(MAIN_WINDOW_ID);
        const bool renderPopup   = m_popupWindow && pending.contains(m_popupWindow->subscribeId());
        const bool renderSubmenu = m_submenuWindow && pending.contains(m_submenuWindow->subscribeId());
        const bool renderDialog  = m_dialogWindow && pending.contains(m_dialogWindow->subscribeId());

        // 1. Render main window
        if (renderMain) {
            renderMainWindow(true);
            notifyMainWindowChanged();
            drawPopupComposite();
            swapMainWindow();
        }

        // 2-4. Render popup-type windows
        if (renderPopup && m_popupWindow) {
            renderPopupWindow(*m_popupWindow, m_popupComposite);
        }
        if (renderSubmenu && m_submenuWindow) {
            renderPopupWindow(*m_submenuWindow, m_submenuComposite);
        }
        if (renderDialog && m_dialogWindow) {
            renderPopupWindow(*m_dialogWindow, m_dialogComposite);
        }
    }

    void swapMainWindow()
    {
        if (m_mainWindow) {
            m_mainWindow->swapBuffers();
        }
    }

    /** @brief Force main window GL flush (makeCurrent + swap) for EGL surface resize */
    void flushMainWindow()
    {
        if (m_mainWindow) {
            m_mainWindow->makeCurrent();
            m_mainWindow->swapBuffers();
        }
    }

    /**
     * @brief Re-render main window with composite textures and swap.
     *
     * Used after popup renders in composite mode. Does NOT notify
     * subscribers -- avoids infinite recapture loop.
     */
    void refreshComposite()
    {
        if (m_uiRenderer == nullptr || !m_mainWindow) {
            return;
        }
        m_mainWindow->makeCurrent();
        m_mainWindow->clear();
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        Ui::Gl::SvgRenderer::setUploadPremultiplied(true);
        m_uiRenderer->Render(m_windowWidth, m_windowHeight);
        if (g_config.isCompositing) {
            drawCompositeTextures();
        }
        m_mainWindow->swapBuffers();
    }

    void renderMainAndSwap()
    {
        renderMainWindow(true);
        drawPopupComposite();
        swapMainWindow();
    }

    void renderPopupWindow(Ui::Window::Popup::PopupWindow & window, CompositeTexture & texture)
    {
        window.render();
        if (g_config.isCompositing) {
            captureComposite(window, texture);
            refreshComposite();
        } else {
            window.swapBuffers();
        }
        m_mainWindow->makeCurrent();
    }

    void recapturePopupCorners() { recaptureCorners(*m_popupWindow, m_popupComposite); }

    void recaptureSubmenuCorners() { recaptureCorners(*m_submenuWindow, m_submenuComposite); }

    void recaptureCorners(Ui::Window::Popup::PopupWindow & window, CompositeTexture & texture)
    {
        if (!window.hasRenderer()) {
            return;
        }

        const auto & cb = window.bound();
        const auto & cr = window.cornerRadii();

        std::array<std::vector<uint8_t>, 4> cornerPixels;
        Ui::Res::Type::border_t             capturedRadii {};
        captureCornerPixels(cb, cr, window.background(), cornerPixels, capturedRadii);

        window.makeCurrent();
        window.popupRenderer().setCornerPixels(std::move(cornerPixels), capturedRadii);
        window.requestRender();
        window.render();

        if (g_config.isCompositing) {
            captureComposite(window, texture);
        }

        m_mainWindow->makeCurrent();
    }

    /**
     * @brief Capture 4 corner regions from the main window back buffer.
     *
     * Must be called after renderMainWindow() and before swapMainWindow()
     * so the back buffer has rendered content. Composites content surface
     * pixels when a corner overlaps one.
     *
     * Pixel data is returned in top-left origin (RGBA, 4 bytes/pixel).
     */

    /**
     * @brief Classify which popup-type window an event belongs to.
     *
     * In composite mode: checks bounds (topmost first: dialog > submenu > popup).
     * In non-composite mode: matches sourceWindow handle against known windows.
     * Sets event.isPopupEvent, event.childWindowId, and adjusts mouse coords.
     * @return Target popup window, or nullptr if event is for main/child window.
     */
    Ui::Window::Popup::PopupWindow * classifyPopupEvent(Event & event)
    {
        const bool isMouse = (event.type == EventType::MouseMove || event.type == EventType::MouseButtonPress
                              || event.type == EventType::MouseButtonRelease || event.type == EventType::Scroll
                              || event.type == EventType::MouseLeave);

        if (g_config.isCompositing) {
            // Composite mode: popups are offscreen, classify by bounds.
            // MouseLeave has no meaningful coords - let dispatchEvent handle it globally.
            if (!isMouse || event.type == EventType::MouseLeave) {
                return nullptr;
            }

            // Check topmost first: dialog > submenu > popup
            const std::array<Ui::Window::Popup::PopupWindow *, 3> candidates = {
                m_dialogWindow.get(),
                m_submenuWindow.get(),
                m_popupWindow.get(),
            };

            for (auto * candidate : candidates) {
                if (candidate != nullptr) {
                    const Ui::Res::Type::bound_t & b = candidate->bound();
                    if (b.contains(event.mouse.x, event.mouse.y)) {
                        event.mouse.x -= static_cast<int>(b.x);
                        event.mouse.y -= static_cast<int>(b.y);
                        event.isPopupEvent = true;
                        return candidate;
                    }
                }
            }
            return nullptr;
        }

        // Non-composite mode: classify by native window handle
        const auto src = event.sourceWindow;
        if (src == 0 || (m_mainWindow && src == m_mainWindow->nativeHandle())) {
            if constexpr (WM_ROUTE_DEBUG) {
                if (isMouse) {
                    std::cout << "[WM] route=main src=" << src
                              << " main=" << (m_mainWindow ? m_mainWindow->nativeHandle() : NativeWindowHandle {})
                              << " type=" << static_cast<int>(event.type) << std::endl;
                }
            }
            return nullptr;
        }

        // Check popup-type windows
        if (m_dialogWindow && src == m_dialogWindow->nativeHandle()) {
            event.isPopupEvent = true;
            return m_dialogWindow.get();
        }
        if (m_submenuWindow && src == m_submenuWindow->nativeHandle()) {
            event.isPopupEvent = true;
            return m_submenuWindow.get();
        }
        if (m_popupWindow && src == m_popupWindow->nativeHandle()) {
            event.isPopupEvent = true;
            return m_popupWindow.get();
        }

        // Check content surfaces (host viewports)
        for (const auto & [id, entry] : m_contentSurfaces) {
            if (entry.window != nullptr && src == entry.window->nativeHandle()) {
                event.childWindowId = id;
                if constexpr (WM_ROUTE_DEBUG) {
                    if (isMouse) {
                        std::cout << "[WM] route=content src=" << src << " childId=" << id
                                  << " type=" << static_cast<int>(event.type) << std::endl;
                    }
                }
                return nullptr;
            }
        }

        if constexpr (WM_ROUTE_DEBUG) {
            if (isMouse) {
                std::cout << "[WM] route=unknown src=" << src << " type=" << static_cast<int>(event.type) << std::endl;
            }
        }
        return nullptr;
    }

    /**
     * @brief Route a popup event to the target popup window.
     */
    void routePopupEvent(const Event & event, Ui::Window::Popup::PopupWindow & target)
    {
        switch (event.type) {
        case EventType::MouseMove: {
            const bool insideVisual = target.containsPoint(event.mouse.x, event.mouse.y);
            if (insideVisual) {
                if (target.onMouseMove(event.mouse.x, event.mouse.y)) {
                    target.requestRender();
                }
                m_popupEventConsumed = true;
                if (m_mainWindow && m_mainWindow->onMouseLeave()) {
                    requestMainRender();
                }
            } else if (target.onMouseLeave()) {
                target.requestRender();
            }
            break;
        }
        case EventType::MouseButtonPress:
            if (event.mouse.button == MouseButton::Left) {
                const bool insideVisual = target.containsPoint(event.mouse.x, event.mouse.y);
                if (insideVisual) {
                    target.onMousePress(event.mouse.x, event.mouse.y, event.mouse.clickCount);
                    target.requestRender();
                    m_popupEventConsumed = true;
                }
            }
            break;
        case EventType::MouseButtonRelease:
            if (event.mouse.button == MouseButton::Left) {
                const bool insideVisual = target.containsPoint(event.mouse.x, event.mouse.y);
                if (insideVisual) {
                    m_lastClickResult = target.onMouseRelease(event.mouse.x, event.mouse.y);
                    target.requestRender();
                    m_popupEventConsumed = true;
                }
            }
            break;
        case EventType::Scroll: {
            const bool insideVisual = target.containsPoint(event.mouse.x, event.mouse.y);
            if (insideVisual) {
                if (target.onScroll(event.mouse.x, event.mouse.y, event.scroll.deltaY)) {
                    target.requestRender();
                }
                m_popupEventConsumed = true;
            }
            break;
        }
        case EventType::MouseLeave:
            target.onMouseLeave();
            target.requestRender();
            break;
        default: break;
        }
    }

    bool captureCornerPixels(const Ui::Res::Type::bound_t &        popupBound,
                             const Ui::Res::Type::border_t &       cornerRadii,
                             const Ui::Color &                     bgColor,
                             std::array<std::vector<uint8_t>, 4> & outPixels,
                             Ui::Res::Type::border_t &             outRadii)
    {
        if (!m_mainWindow) {
            return false;
        }

        const int parentW = static_cast<int>(m_windowWidth);
        const int parentH = static_cast<int>(m_windowHeight);

        outRadii = clampRadii(cornerRadii, popupBound.w, popupBound.h);

        bool anyCapture = false;
        m_mainWindow->makeCurrent();

        // Corner capture rects and buffers (indexed TL=0, TR=1, BR=2, BL=3)
        std::array<Ui::Res::Type::bound_t, 4> caps {};

        for (id_t i = 0; i < 4; ++i) {
            const auto radius = static_cast<int>(Ui::Gl::Rounded::borderRadius(outRadii, i));
            if (radius <= 0) {
                outPixels.at(i).clear();
                continue;
            }

            const int popX = static_cast<int>(popupBound.x);
            const int popY = static_cast<int>(popupBound.y);
            const int popW = static_cast<int>(popupBound.w);
            const int popH = static_cast<int>(popupBound.h);

            // Corner origin in parent coords
            int cx = 0;
            int cy = 0;
            switch (i) {
            case 0:
                cx = popX;
                cy = popY;
                break;
            case 1:
                cx = popX + popW - radius;
                cy = popY;
                break;
            case 2:
                cx = popX + popW - radius;
                cy = popY + popH - radius;
                break;
            case 3:
                cx = popX;
                cy = popY + popH - radius;
                break;
            default: continue;
            }

            // Clamp capture rect to parent bounds
            const int capX = std::max(0, cx);
            const int capY = std::max(0, cy);
            int       capW = std::min(radius, parentW - capX);
            int       capH = std::min(radius, parentH - capY);
            if (cx < 0) {
                capW = std::min(capW, radius + cx);
            }
            if (cy < 0) {
                capH = std::min(capH, radius + cy);
            }
            capW = std::max(0, capW);
            capH = std::max(0, capH);

            caps.at(i) = { static_cast<fpx_t>(capX),
                           static_cast<fpx_t>(capY),
                           static_cast<fpx_t>(capW),
                           static_cast<fpx_t>(capH) };

            // Init buffer with bg color
            auto & buf = outPixels.at(i);
            buf.resize(static_cast<size_t>(radius) * radius * 4);
            for (int j = 0; j < radius * radius; ++j) {
                buf.at(static_cast<size_t>(j) * 4 + 0) = bgColor.r();
                buf.at(static_cast<size_t>(j) * 4 + 1) = bgColor.g();
                buf.at(static_cast<size_t>(j) * 4 + 2) = bgColor.b();
                buf.at(static_cast<size_t>(j) * 4 + 3) = 255;
            }

            if (capW <= 0 || capH <= 0) {
                anyCapture = true;
                continue;
            }

            // Read from GL back buffer (bottom-left origin), flip to top-left
            std::vector<uint8_t> regionPixels(static_cast<size_t>(capW * capH * 4));
            glReadPixels(capX, parentH - capY - capH, capW, capH, GL_RGBA, GL_UNSIGNED_BYTE, regionPixels.data());

            const int rowBytes = capW * 4;
            int       row      = 0;
            while (row < capH / 2) {
                std::swap_ranges(&regionPixels[row * rowBytes],
                                 &regionPixels[row * rowBytes + rowBytes],
                                 &regionPixels[(capH - 1 - row) * rowBytes]);
                ++row;
            }

            // Blit into corner buffer at offset
            const int offX = capX - cx;
            const int offY = capY - cy;
            for (int py = 0; py < capH; ++py) {
                for (int px = 0; px < capW; ++px) {
                    const int dX = px + offX;
                    const int dY = py + offY;
                    if (dX < 0 || dX >= radius || dY < 0 || dY >= radius) {
                        continue;
                    }
                    const int srcIdx                     = (py * capW + px) * 4;
                    const int dstIdx                     = (dY * radius + dX) * 4;
                    buf[static_cast<size_t>(dstIdx) + 0] = regionPixels[static_cast<size_t>(srcIdx) + 0];
                    buf[static_cast<size_t>(dstIdx) + 1] = regionPixels[static_cast<size_t>(srcIdx) + 1];
                    buf[static_cast<size_t>(dstIdx) + 2] = regionPixels[static_cast<size_t>(srcIdx) + 2];
                    buf[static_cast<size_t>(dstIdx) + 3] = 255;
                }
            }

            anyCapture = true;
        }

        // Composite content surface pixels over corners that overlap a viewport
        for (id_t i = 0; i < 4; ++i) {
            const int radius = static_cast<int>(Ui::Gl::Rounded::borderRadius(outRadii, i));
            const int capX   = static_cast<int>(caps.at(i).x);
            const int capY   = static_cast<int>(caps.at(i).y);
            const int capW   = static_cast<int>(caps.at(i).w);
            const int capH   = static_cast<int>(caps.at(i).h);
            if (radius <= 0 || capW <= 0 || capH <= 0) {
                continue;
            }

            const int popX = static_cast<int>(popupBound.x);
            const int popY = static_cast<int>(popupBound.y);
            const int popW = static_cast<int>(popupBound.w);
            const int popH = static_cast<int>(popupBound.h);

            int cx = 0;
            int cy = 0;
            switch (i) {
            case 0:
                cx = popX;
                cy = popY;
                break;
            case 1:
                cx = popX + popW - radius;
                cy = popY;
                break;
            case 2:
                cx = popX + popW - radius;
                cy = popY + popH - radius;
                break;
            case 3:
                cx = popX;
                cy = popY + popH - radius;
                break;
            default: continue;
            }
            const int offX = capX - cx;
            const int offY = capY - cy;

            // Composite the active content surface's pixels into this corner
            // where it overlaps (only the active, ready surface is visible).
            auto compositeContent = [&](const Ui::Res::Type::bound_t & contentBound, Ui::IRenderer & renderer) {
                const int oX = static_cast<int>(contentBound.x);
                const int oY = static_cast<int>(contentBound.y);
                const int oW = static_cast<int>(contentBound.w);
                const int oH = static_cast<int>(contentBound.h);

                if (capX + capW <= oX || capX >= oX + oW || capY + capH <= oY || capY >= oY + oH) {
                    return;
                }

                int                  contentW      = 0;
                int                  contentH      = 0;
                std::vector<uint8_t> contentPixels = renderer.readPixels(contentW, contentH);
                if (contentPixels.empty() || contentW <= 0 || contentH <= 0) {
                    return;
                }

                auto & buf = outPixels.at(i);
                for (int py = 0; py < capH; ++py) {
                    for (int px = 0; px < capW; ++px) {
                        const int sx = capX - oX + px;
                        const int sy = capY - oY + py;
                        if (sx < 0 || sx >= contentW || sy < 0 || sy >= contentH) {
                            continue;
                        }
                        const int dX = px + offX;
                        const int dY = py + offY;
                        if (dX < 0 || dX >= radius || dY < 0 || dY >= radius) {
                            continue;
                        }
                        const int srcIdx                     = (sy * contentW + sx) * 4;
                        const int dstIdx                     = (dY * radius + dX) * 4;
                        buf[static_cast<size_t>(dstIdx) + 0] = contentPixels[static_cast<size_t>(srcIdx) + 0];
                        buf[static_cast<size_t>(dstIdx) + 1] = contentPixels[static_cast<size_t>(srcIdx) + 1];
                        buf[static_cast<size_t>(dstIdx) + 2] = contentPixels[static_cast<size_t>(srcIdx) + 2];
                        buf[static_cast<size_t>(dstIdx) + 3] = 255;
                    }
                }
            };
            if (auto cit = m_contentSurfaces.find(m_activeContent);
                cit != m_contentSurfaces.end() && cit->second.window != nullptr && cit->second.renderer != nullptr
                && cit->second.isReady) {
                compositeContent(cit->second.window->bound(), *cit->second.renderer);
            }
        }

        m_mainWindow->makeCurrent();
        return anyCapture;
    }

    /**
     * @brief Check if Ui::Render::UiRenderer is initialized
     */
    [[nodiscard]] bool hasUiRenderer() const { return m_uiRenderer != nullptr; }

    [[nodiscard]] bool isRunning() const { return m_running.load(); }
    void               stop() { m_running = false; }

    /**
     * @brief Poll for platform events (non-blocking)
     * @return true if an event was retrieved, false if no events pending
     */
    bool pollEvent(Event & event)
    {
        if (!m_eventHandler) {
            return false;
        }
        return m_eventHandler->pollEvent(event, *m_mainWindow);
    }

    /**
     * @brief Check if there are pending events
     */
    [[nodiscard]] bool hasPendingEvents() const
    {
        if (!m_eventHandler) {
            return false;
        }
        return m_eventHandler->hasPendingEvents(*m_mainWindow);
    }

    /**
     * @brief Flush pending output to display server
     */
    void flush()
    {
        if (m_eventHandler) {
            m_eventHandler->flush(*m_mainWindow);
        }
    }

    bool copyToClipboard(const std::string & text)
    {
        if (m_eventHandler && m_mainWindow) {
            return m_eventHandler->copyToClipboard(*m_mainWindow, text);
        }
        return false;
    }

    /**
     * @brief Resize all child windows when main window resizes
     *
     * Called in response to OS ConfigureNotify - the main window has already
     * been resized by the OS, so we only sync internal state (no XResizeWindow).
     */
    void onMainWindowResize(fpx_t width, fpx_t height)
    {
        std::cout << "[WindowManager] onMainWindowResize: " << width << "x" << height << " (was " << m_windowWidth
                  << "x" << m_windowHeight << ")" << std::endl;

        m_windowWidth  = width;
        m_windowHeight = height;

        // Shrinking the window can leave persisted dock widths summing to
        // more than the new viewport can fit, which would force the content surface
        // viewport to clamp at 1px (or invert) below. Clamp each dock against
        // the now-smaller window first; clampStateWidth writes session.json
        // through ResManager::setDockState when a width actually shrinks, so
        // the persisted value tracks what the user can see. Refresh the
        // dock-driven m_uiLeft / m_uiRight against the clamped widths so the
        // content surface math below uses the post-clamp totals.
        for (auto & [id, dock] : m_docks) {
            clampDockStateToViewport(dock);
        }
        const auto & layout = m_resManager.layout();
        m_uiLeft            = toPhys(layout.leftToolbar.width);
        m_uiRight           = toPhys(layout.rightToolbar.width);
        applyDockMargins();

        // Re-position docks now that the window has new dimensions. Dock widths
        // didn't change, only the right-edge cursor (m_windowWidth - m_uiRight)
        // and the body height did - layoutDocks() reads both.
        layoutDocks();

        // Sync main window internal dimensions first - recenter/resize reads bound()
        m_mainWindow->syncDimensions(width, height);

        // Resize UI renderer to new dimensions
        if (m_uiRenderer != nullptr) {
            m_uiRenderer->resize(width, height);
        }

        // Recenter dialog (position only - corner recapture after content surface resize below)
        if (m_dialogWindow) {
            m_dialogWindow->recenter(m_mainWindow->bound(), m_uiTop, m_uiBottom);
        }

        const Ui::Res::Type::bound_t newBound = viewportBound();
        for (auto & [id, entry] : m_contentSurfaces) {
            if (entry.window == nullptr) {
                continue;
            }
            positionContentSurface(*entry.window);

            // Resize the renderer; only redraw + capture the active surface.
            if (entry.renderer != nullptr) {
                entry.renderer->resize(newBound.w, newBound.h);

                const bool isActive = (id == m_activeContent);
                if (isActive) {
                    entry.window->requestRender();
                    entry.window->render();
                    if (g_config.isCompositing) {
                        captureContentComposite(entry);
                    }
                }
            }
        }

        // Recapture dialog corners after content viewports are resized and captured
        if (m_dialogWindow) {
            renderMainWindow(true);
            recaptureCorners(*m_dialogWindow, m_dialogComposite);
        }
    }

    // -------- Ui::IEventApp Interface --------

    bool onMouseMove(int x, int y) override
    {
        // Mid-drag on a dock grip: route exclusively to that dock until
        // mouse-up so the cursor can wander into the content surface without
        // breaking the resize gesture.
        if (m_activeDragDock != Ui::INVALID_ID) {
            auto it = m_docks.find(m_activeDragDock);
            if (it != m_docks.end()) {
                const fpx_t cssX = toCss(x);
                const fpx_t cssY = toCss(y);
                if (it->second.onMouseMove(cssX, cssY)) {
                    // Transient width changed. First clamp to what the
                    // viewport can actually fit (so the dock can't push the
                    // content surface below zero), then reflow margins + dock
                    // positions + content surface size in one pass. The content surface
                    // child shrinks/grows live as the user drags.
                    clampDraggingDockToViewport(it->second);
                    reflowDocksAndViewport();
                }
                return true;
            }
            m_activeDragDock = Ui::INVALID_ID; // dock vanished; release capture
        }

        // Hover update on all docks (cheap, none capture by default).
        bool dockHoverChanged = false;
        for (auto & [id, dock] : m_docks) {
            dockHoverChanged |= dock.onMouseMove(toCss(x), toCss(y));
        }
        if (dockHoverChanged) {
            m_renderQueue.request(MAIN_WINDOW_ID);
        }

        if (auto hit = hitTestActiveContent(x, y); hit.window != nullptr) {
            if (m_mainWindow) {
                m_mainWindow->onMouseLeave();
            }
            return hit.window->onMouseMove(hit.lx, hit.ly);
        }

        // Cursor left the content viewport but stayed in the app; clear the
        // active surface's drag state so it doesn't resume on re-entry.
        if (Ui::IWindow * active = activeContentWindow()) {
            active->onMouseLeave();
        }

        if (m_popupWindow) {
            m_popupWindow->onMouseLeave();
            m_popupWindow->requestRender();
        }
        if (m_submenuWindow) {
            m_submenuWindow->onMouseLeave();
            m_submenuWindow->requestRender();
        }

        return m_mainWindow ? m_mainWindow->onMouseMove(x, y) : dockHoverChanged;
    }

    bool onMousePress(int x, int y, int clickCount) override
    {
        // Dock grip press → capture and start drag, or toggle on double-
        // click. Has to run before content surface routing because grip rects can sit
        // adjacent to (or briefly overlap by a pixel due to rounding) the
        // content surface edge.
        const fpx_t cssX = toCss(x);
        const fpx_t cssY = toCss(y);
        for (auto & [id, dock] : m_docks) {
            if (dock.onMouseDown(cssX, cssY, clickCount)) {
                // Double-click resolves immediately (no drag), so reflow now
                // and don't hold capture. A normal press starts a drag and
                // keeps capture until the corresponding release.
                if (dock.isDragging()) {
                    m_activeDragDock = id;
                } else {
                    // Restore via memoryX may exceed what fits now (other
                    // docks may have grown since the size was captured), so
                    // clamp against current viewport availability before the
                    // layout pass sees the new width.
                    clampDockStateToViewport(dock);
                    reflowDocksAndViewport();
                }
                m_renderQueue.request(MAIN_WINDOW_ID);
                return true;
            }
        }

        if (auto hit = hitTestActiveContent(x, y); hit.window != nullptr) {
            return hit.window->onMousePress(hit.lx, hit.ly, clickCount);
        }
        return m_mainWindow ? m_mainWindow->onMousePress(x, y, clickCount) : false;
    }

    Ui::Render::click_result_t onMouseRelease(int x, int y) override
    {
        // Mid-drag release on a dock: commit state and reflow content surface.
        if (m_activeDragDock != Ui::INVALID_ID) {
            auto it = m_docks.find(m_activeDragDock);
            if (it != m_docks.end()) {
                it->second.onMouseUp(toCss(x));
            }
            m_activeDragDock = Ui::INVALID_ID;
            reflowDocksAndViewport();
            return {};
        }

        if (auto hit = hitTestActiveContent(x, y); hit.window != nullptr) {
            return hit.window->onMouseRelease(hit.lx, hit.ly);
        }
        return m_mainWindow ? m_mainWindow->onMouseRelease(x, y) : Ui::Render::click_result_t {};
    }

    bool onMouseLeave() override
    {
        bool changed = false;

        // Active content surface can be mid-drag; clear its drag state too
        if (Ui::IWindow * active = activeContentWindow()) {
            changed |= active->onMouseLeave();
        }
        if (m_mainWindow) {
            changed = m_mainWindow->onMouseLeave();
        }
        if (m_popupWindow) {
            changed |= m_popupWindow->onMouseLeave();
        }
        if (m_submenuWindow) {
            changed |= m_submenuWindow->onMouseLeave();
        }
        if (m_dialogWindow) {
            changed |= m_dialogWindow->onMouseLeave();
        }

        // Docks: clear hover and finalize any in-flight drag. Ui::Render::DockColumn::onMouseLeave
        // commits the transient width when the drag was meaningful (>= clickThreshold)
        // so a drag-to-zero followed by release outside the window collapses the dock
        // as the user expects; small/no-drag leaves fall through as a no-op.
        // Cursor wandering into the content surface mid-drag doesn't reach here - that's
        // handled by the capture in onMouseMove.
        const bool wasDragging = (m_activeDragDock != Ui::INVALID_ID);
        for (auto & [id, dock] : m_docks) {
            dock.onMouseLeave();
        }
        if (wasDragging) {
            m_activeDragDock = Ui::INVALID_ID;
            // Width may have changed (either committed-on-leave or, below threshold,
            // reverted to start width). Reflow so dock layout + content surface pick up
            // the final state instead of holding the last drag-step size.
            reflowDocksAndViewport();
            changed = true;
        }

        return changed;
    }

    bool onScroll(int x, int y, fpx_t deltaY) override
    {
        // Scroll passes raw (main-local) coords, matching the prior contract -
        // the renderer uses scroll for zoom, not a hit point.
        if (auto hit = hitTestActiveContent(x, y); hit.window != nullptr) {
            return hit.window->onScroll(x, y, deltaY);
        }
        return m_mainWindow ? m_mainWindow->onScroll(x, y, deltaY) : false;
    }

    // Child-routed overloads (child-local coordinates, bypass hit-testing).
    // childId is a content-surface id; coords are already in its frame.

    bool onMouseMove(int x, int y, id_t childId)
    {
        Ui::IWindow * window = (childId != Ui::INVALID_ID) ? contentWindow(childId) : nullptr;
        return window != nullptr ? window->onMouseMove(x, y) : onMouseMove(x, y);
    }

    bool onMousePress(int x, int y, id_t childId, int clickCount)
    {
        Ui::IWindow * window = (childId != Ui::INVALID_ID) ? contentWindow(childId) : nullptr;
        return window != nullptr ? window->onMousePress(x, y, clickCount) : onMousePress(x, y, clickCount);
    }

    Ui::Render::click_result_t onMouseRelease(int x, int y, id_t childId)
    {
        Ui::IWindow * window = (childId != Ui::INVALID_ID) ? contentWindow(childId) : nullptr;
        return window != nullptr ? window->onMouseRelease(x, y) : onMouseRelease(x, y);
    }

    bool onScroll(int x, int y, fpx_t deltaY, id_t childId)
    {
        Ui::IWindow * window = (childId != Ui::INVALID_ID) ? contentWindow(childId) : nullptr;
        return window != nullptr ? window->onScroll(x, y, deltaY) : onScroll(x, y, deltaY);
    }

    /**
     * @brief Shutdown all child windows and cleanup
     */
    void shutdown()
    {
        if (!m_mainWindow) {
            return;
        }

        std::cout << "[WindowManager] Shutting down..." << std::endl;

        m_running = false;

        destroyPopup(true);
        m_dialogWindow.reset();

        // Clean up GL resources in main window's context
        m_mainWindow->makeCurrent();
        cleanupCompositeShader();
        m_popupComposite.destroy();
        m_submenuComposite.destroy();
        m_dialogComposite.destroy();
        // Content surfaces are owned by the host; we only drop our
        // composite textures + non-owning registry entries (windows already gone
        // or about to be torn down by the host).
        for (auto & [id, entry] : m_contentSurfaces) {
            entry.composite.destroy();
        }
        m_contentSurfaces.clear();

        // Drop docks before ResManager nulls below; they hold const references
        // into ResManager's layout.
        m_docks.clear();

        m_uiRenderer = nullptr; // mainWindow owns renderer; dtor cleans it up

        // Destroy event handler before main window - pointer/keyboard proxies
        // require a live display connection
        m_eventHandler.reset();

        // Main window dtor handles renderer cleanup + EGL/display destroy
        m_mainWindow.reset();

        std::cout << "[WindowManager] Shutdown complete" << std::endl;
    }

    /**
     * @brief Render the active content surface if queued.
     *
     * Call from the main loop. Returns true if a composite capture happened and
     * the main window must re-render (Wayland composite path).
     */
    bool renderContentSurfaces()
    {
        bool anyRendered = false;
        for (auto & [id, entry] : m_contentSurfaces) {
            if (entry.window == nullptr) {
                continue;
            }

            // Skip while the host is mid async-load (avoids empty-scene renders
            // from spurious resize events), and render only the active surface.
            if (!entry.isReady || id != m_activeContent) {
                continue;
            }

            // Only render if in the render queue.
            if (!m_renderQueue.pending().contains(WS_GROUP_ID + id)) {
                continue;
            }

            const bool rendered = entry.window->render();

#ifdef HAVE_WAYLAND
            // Native Wayland: the surface uses our subsurface's EGL context but
            // can't swap (no EGL surface of its own). Restore + swap manually.
            if (rendered) {
                entry.window->makeCurrent();
                entry.window->swapBuffers();
            }
#endif

            // Composite mode: capture rendered pixels for main-window compositing.
            if (rendered && g_config.isCompositing) {
                captureContentComposite(entry);
                anyRendered = true;
            }
        }
        return anyRendered;
    }

    void initCompositeShader()
    {
        if (m_compositeProgram != 0) {
            return;
        }

        const char * vertexSource = R"GLSL(
            #version 330 core
            layout(location = 0) in vec2 aPos;
            layout(location = 1) in vec2 aUV;
            out vec2 vUV;
            uniform mat4 u_projection;
            void main() {
                gl_Position = u_projection * vec4(aPos, 0.0, 1.0);
                vUV = aUV;
            }
        )GLSL";

        const char * fragmentSource = R"GLSL(
            #version 330 core
            in vec2 vUV;
            out vec4 fragColor;
            uniform sampler2D u_tex;
            void main() {
                fragColor = texture(u_tex, vUV);
            }
        )GLSL";

        const GLuint vert = Ui::Gl::Util::compileShader(GL_VERTEX_SHADER, vertexSource, "[Composite] Vertex error: ");
        const GLuint frag = Ui::Gl::Util::compileShader(GL_FRAGMENT_SHADER,
                                                        fragmentSource,
                                                        "[Composite] Fragment error: ");
        if (vert == 0 || frag == 0) {
            if (vert != 0) {
                glDeleteShader(vert);
            }
            if (frag != 0) {
                glDeleteShader(frag);
            }
            return;
        }

        m_compositeProgram = Ui::Gl::Util::linkProgram(vert, frag, "[Composite] Link error: ");
        if (m_compositeProgram == 0) {
            return;
        }

        m_compositeUProjection = glGetUniformLocation(m_compositeProgram, "u_projection");
        m_compositeUTex        = glGetUniformLocation(m_compositeProgram, "u_tex");

        Ui::Gl::Util::createPosUvVao(m_compositeVao, m_compositeVbo);
    }

    void cleanupCompositeShader()
    {
        Ui::Gl::Util::deleteProgram(m_compositeProgram);
        Ui::Gl::Util::deleteBuffer(m_compositeVbo);
        Ui::Gl::Util::deleteVertexArray(m_compositeVao);
    }

    /**
     * @brief Draw the active content surface's captured texture into the main
     * window back buffer (Wayland composite mode). Opaque (no blend).
     */
    void drawContentComposite()
    {
        auto it = m_contentSurfaces.find(m_activeContent);
        if (it == m_contentSurfaces.end() || it->second.window == nullptr) {
            return;
        }
        content_surface_t & entry = it->second;
        if (entry.composite.pixels.empty() || entry.composite.w <= 0 || entry.composite.h <= 0) {
            return;
        }
        blitChildTexture(entry.composite.texture,
                         entry.composite.pixels,
                         entry.composite.w,
                         entry.composite.h,
                         entry.composite.dirty,
                         entry.window->bound(),
                         false);
    }

    /**
     * @brief Draw popup texture on the main window (composite mode)
     */
    void drawCompositeTexture(CompositeTexture & texture, const Ui::Res::Type::bound_t & bound)
    {
        blitChildTexture(texture.texture, texture.pixels, texture.w, texture.h, texture.dirty, bound, true);
    }

    /**
     * @brief Upload (if dirty) and blit a captured surface texture onto the main
     * back buffer at `dest`, using the shared composite shader. Texture handle +
     * dirty flag are caller-owned so any content surface can reuse this. `blend`
     * is true for translucent popups, false for opaque content viewports.
     */
    void blitChildTexture(GLuint &                       tex,
                          const std::vector<uint8_t> &   pixels,
                          int                            srcWidth,
                          int                            srcHeight,
                          bool &                         dirty,
                          const Ui::Res::Type::bound_t & dest,
                          bool                           blend)
    {
        initCompositeShader();
        if (m_compositeProgram == 0) {
            return;
        }

        if (tex == 0) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        if (dirty) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, srcWidth, srcHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            dirty = false;
        }

        glUseProgram(m_compositeProgram);
        const auto projection = Ui::Gl::Util::orthoProjection(m_windowWidth, m_windowHeight);
        glUniformMatrix4fv(m_compositeUProjection, 1, GL_FALSE, projection.data());
        glActiveTexture(GL_TEXTURE0);
        glUniform1i(m_compositeUTex, 0);

        if (blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            glDisable(GL_BLEND);
        }

        glBindVertexArray(m_compositeVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_compositeVbo);
        glBindTexture(GL_TEXTURE_2D, tex);

        Ui::Gl::Util::drawTriangles(Ui::Gl::Util::quadVerticesFlipY(dest.x, dest.y, dest.w, dest.h), 6);

        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        glUseProgram(0);
    }

    /**
     * @brief Draw all composite textures (content surface + popup) -- used by refreshComposite()
     */
    void drawCompositeTextures()
    {
        drawContentComposite();
        drawPopupComposite();
    }
};

} // namespace Ui::Window
