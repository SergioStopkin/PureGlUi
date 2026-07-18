# ui/window + ui/pubsub - windowing backend, coordinator, pub/sub

The `Ui::Window` namespace is the platform windowing backend (per-OS surfaces, EGL context, events) plus the coordinator `WindowManager` that owns the main window, popups/submenus/dialogs, the event loop, the render queue, and the content-surface registry. `Ui::PubSub` is the small in-process publish/subscribe dispatcher the coordinator wires everything through. The coordinator drives `[main window] + content surfaces` blind through `Ui::IWindow` / `Ui::IRenderer` / `Ui::IEventApp` (see [interfaces.md](interfaces.md)) and reads scale / compositing from `Ui::g_config` (see [vocabulary.md](vocabulary.md)).

## Coordinator (WindowManager, Connector)

### Ui::Window::WindowManager

Header: `include/ui/window/windowmanager.h`

`final class WindowManager : public Ui::IEventApp, private Common::NonCopyable`. The windowing coordinator. It runs on the main thread and owns: the main window+chrome pairing, the dock columns, the popup/submenu/dialog windows, the content-surface registry, the platform event handler, the pub/sub dispatcher, and the render queue. It never references host domain types - content surfaces are registered as bare `IWindow` + `IRenderer` pointers.

Construction and lifecycle:

```cpp
explicit WindowManager(const Ui::Res::ResManager & resManager);
~WindowManager() override;                 // calls shutdown()
bool initialize();                         // create main window + GL + docks + event handler
void shutdown();                           // tear down popups, docks, event handler, main window
[[nodiscard]] bool isRunning() const;
void stop();
```

- `WindowManager(resManager)` - captures a non-owning const reference to `ResManager` (the owner, `Shell`/host, guarantees its lifetime).
- `initialize()` - resolves compositing mode (`g_config.isCompositing`: `WAYLAND_DISPLAY` selects the XWayland composite workaround when Wayland is not compiled in), creates the main window at the layout/session size and position, loads the GL entry points, requires GL >= 3.3 (fails with a clear message on a legacy context), builds one `DockColumn` per parsed dock config, computes DPI margins, creates the platform event handler and wires its child-window lookup to `childIdForHandle`. Returns `false` on any window/GL failure.
- `shutdown()` - destroys popups and dialog, frees composite GL textures in the main context, drops content-surface registry entries (host owns the surfaces), clears docks, destroys the event handler before the main window, then resets the main connector (renderer torn down before its GL context). Idempotent (no-op if no main window).

Accessors:

```cpp
Ui::PubSub::Subscribe &   subscribe();
Ui::Window::RenderQueue & renderQueue();
[[nodiscard]] NativeWindow & mainWindow();
[[nodiscard]] fpx_t windowWidth() const;
[[nodiscard]] fpx_t windowHeight() const;
[[nodiscard]] Ui::Res::Type::bound_t viewportBound() const;   // rect left for content after chrome + docks
static constexpr id_t APP_SUBSCRIBER_ID = Ui::PubSub::subscriberId(Ui::PubSub::SubscriberId::App);
```

App-facing `std::function` setters (host hooks):

```cpp
void setOnDialogClose(Ui::task_fn_t callback);
void setDialogContentResolver(std::function<std::wstring(const std::string &)> resolver);
void setOnElementHover(std::function<void(Ui::Render::UiElementType, id_t)> cb);
void setDoubleClickConfig(uint32_t intervalMs, int distancePx);
```

- `setOnDialogClose` - fired (deferred) when a dialog closes; the host then reads `lastDialogAction()` / `lastDialogData()`.
- `setDialogContentResolver` - resolves dialog content placeholders (e.g. `%VERSION%`/`%CPU%`) to runtime values; if unset the locale string is shown verbatim.
- `setOnElementHover` - forwarded to the main `UiRenderer`; reports `(elementType, id)` on hover.
- `setDoubleClickConfig` - forwards double-click interval/distance thresholds to the platform event handler (host supplies these from its viewport config).

Content-surface registry (the host-facing seam) - see "Content surfaces + render queue" below:

```cpp
void addContentSurface(id_t id, Ui::IWindow & window, Ui::IRenderer & pairing);
void removeContentSurface(id_t id);
void setActiveContentSurface(id_t id);
void setContentSurfaceReady(id_t id, bool isReady);
[[nodiscard]] bool isContentReady(id_t id) const;
[[nodiscard]] id_t childIdForHandle(Ui::Window::NativeWindowHandle handle) const;
```

- `addContentSurface(id, window, pairing)` - register a host-owned surface. The coordinator wires its render request into the queue and positions it in the viewport. `window` and `pairing` must outlive the registration (non-owning).
- `removeContentSurface(id)` - drop the entry and free its composite texture in the main GL context; clears the active surface if it was this one.
- `setActiveContentSurface(id)` - make one surface visible/interactive; the others are hidden (windows overlap). In composite mode the surfaces stay offscreen and only the active one is repainted.
- `setContentSurfaceReady(id, isReady)` - the host clears this while a model loads asynchronously so the render loop skips the surface (avoids empty-scene frames), and sets it once the load lands.
- `isContentReady(id)` - false while mid async-load; unknown id -> false.
- `childIdForHandle(handle)` - resolve a native handle to its content-surface id (or `INVALID_ID` for the main window / an unregistered handle). The single source the event peers' child-window lookup is wired to.

Event dispatch and the frame pipeline:

```cpp
void dispatchEvent(const Event & incoming);   // classify + route + notify subscribers
bool pollEvent(Event & event);
[[nodiscard]] bool hasPendingEvents() const;
void flush();
bool copyToClipboard(const std::string & text);
void renderAll();                             // once per frame: content surfaces + refresh + drain queue
void onMainWindowResize(fpx_t width, fpx_t height);
bool refreshDisplayMetrics();                 // re-query DPI, recompute margins; true if scale changed
void apply(Ui::Res::Type::Changed changed);   // push resource changes to all windows/renderers
```

`WindowManager` implements the `Ui::IEventApp` pointer-event sink (`onMouseMove`/`onMousePress`/`onMouseRelease`/`onMouseLeave`/`onScroll`, plus child-routed overloads taking an `id_t childId`). Incoming OS events flow `pollEvent -> dispatchEvent`, which classifies the source (dialog is modal and wins; then popup/submenu; then main/content surface), rebases coordinates, routes to the right sink, and finally notifies subscribers via `Ui::PubSub::eventSourceId(EventType)` - the shell subscribes there for its policy decisions (menu management, shortcuts, popup close). Dock grip drag is captured exclusively until mouse-up.

Popup / submenu / dialog orchestration is a large internal surface (`createPopup`/`initPopupRenderer`/`destroyPopup`, `createSubmenu`/`onSubmenuHover`/`destroySubmenu`, `openDialog`/`closeDialog`/`confirmDialog`/`dismissDialog`/`dialogKeyPress`). These are driven by `Ui::Shell`; hosts use the shell API rather than calling them directly. Query helpers include `hasPopup()`, `popupFollowsParent()`, `hasSubmenu()`, `hasDialog()`, `lastDialogAction()`, `lastDialogData()`.

### Ui::Window::Connector

Header: `include/ui/window/connector.h`

```cpp
template <Window TWindow, Renderer TRenderer>
class Connector final : public Ui::IRenderer, private Common::NonCopyable;
```

Binds one window to one renderer. Windows and renderers are independent families that never reference each other; the `Connector` is the only place a pairing exists and the only home for the glue that needs both sides (make-current-then-draw, event-to-redraw coupling). It implements `Ui::IRenderer` itself, so the coordinator can drive any pairing type-erased. Two concepts constrain the template parameters:

```cpp
template <typename T> concept Window   = requires(T w) { w.makeCurrent(); w.swapBuffers(); w.requestRender(); };
template <typename T> concept Renderer = std::derived_from<T, Ui::IRenderer>;
```

Lifetime is structural: the renderer member is declared after the window, so it is destroyed first - GL teardown always runs while the window's context is alive. The renderer is emplaced after `window.create()` because renderers need a live GL context to construct.

Public API:

```cpp
template <typename... WindowArgs> explicit Connector(WindowArgs &&... windowArgs);   // constructs the window
~Connector() override;                                                               // resetRenderer()

[[nodiscard]] TWindow &       window();
[[nodiscard]] const TWindow & window() const;

template <typename... RendererArgs> TRenderer & emplaceRenderer(RendererArgs &&... rendererArgs);
[[nodiscard]] bool        hasRenderer() const;
[[nodiscard]] TRenderer & renderer();
void                      resetRenderer();
```

- `Connector(windowArgs...)` - forwards to the window constructor (e.g. `(subscribe, subscribeId)`).
- `emplaceRenderer(rendererArgs...)` - makes the window's GL context current, then constructs the renderer in place and returns it. Call after `window().create()`.
- `resetRenderer()` - explicit early teardown: makes the context current, then destroys the renderer.

`Ui::IRenderer` overrides (`render`/`refresh`/`resize`/`apply`/`cleanup`/`readPixels`) each make the window's context current, then forward to the renderer (no-op / default when no renderer). `render()` does NOT swap - callers present explicitly (`swapBuffers` or a composite capture). `Ui::IEventApp` overrides (`onMouseMove`/`onMousePress`/`onMouseRelease`/`onMouseLeave`/`onScroll`) forward to the renderer and call `window().requestRender()` when the renderer reports a change.

## Backend primitives

### Ui::Window::WindowBase

Header: `include/ui/window/windowbase.h`

```cpp
template <typename Derived>
class WindowBase : public Ui::IWindow, private Common::NonCopyable;
```

CRTP base for the platform windows. Holds the members and behavior shared across every backend (`X11Window`, `WaylandWindow`, `Win32Window`, `MacOsWindow`); the derived class implements the platform-specific `Ui::IWindow` methods and is reached via the `derived()` helper.

```cpp
explicit WindowBase(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID);
~WindowBase() override;                                     // removes m_subscribeId from Subscribe

void setPosition(fpx_t x, fpx_t y) override;
virtual void setWindowIcon(const std::string & mainIconPath, const std::string & symbolicIconPath);  // default no-op
[[nodiscard]] Ui::Res::Type::bound_t bound() const override;
[[nodiscard]] bool isHardwareGl() const override;
void refreshHardwareGl();                                   // folds the runtime GL_RENDERER check into the flag
void setBackground(const Ui::Color & color) override;
void setRoundedCorners(bool enabled, fpx_t radius) override;

void setSubscribeId(id_t subscribeId);
[[nodiscard]] id_t                    subscribeId() const;
[[nodiscard]] Ui::PubSub::Subscribe & subscribe();

void setRenderRequest(Ui::task_fn_t fn) override;
void requestRender() override;                              // invokes the render-request callback
void syncDimensions(fpx_t width, fpx_t height);            // sync geometry on OS resize (no OS resize call)
```

- Subscribe ownership is RAII: the destructor calls `subscribe.remove(subscribeId)` so a torn-down window unwires its render-queue entry automatically.
- `isHardwareGl()` defaults true; macOS flips it false on software-renderer fallback. `refreshHardwareGl()` additionally folds in the runtime `GL_RENDERER` check (Mesa llvmpipe, SwiftShader, GDI Generic name themselves there) - the coordinator calls it after context creation so the flag is authoritative.
- `syncDimensions` updates the cached `bound()` and requests a render without calling the OS resize (use it when responding to OS resize events to avoid feedback loops).

### Ui::Window::NativeWindow

Header: `include/ui/window/nativewindow.h`

Compile-time alias to the current platform's `Ui::IWindow` implementation: `Platform::Win32Window` (`_WIN32`), `Platform::MacOsWindow` (`__APPLE__`), `Platform::WaylandWindow` (`HAVE_WAYLAND`), or `Platform::X11Window` (`HAVE_X11`). A `#error` fires if no window system is configured.

### Ui::Window::NativeWindowHandle + child_id_fn_t

Header: `include/ui/window/nativewindowhandle.h`

Lightweight header (no platform window classes, to avoid circular includes). Aliases the per-platform native handle type: `HWND` (Win32), `NSView *` (macOS), `wl_surface *` (Wayland), `::Window` (X11).

```cpp
using child_id_fn_t = std::function<id_t(NativeWindowHandle)>;
```

`child_id_fn_t` resolves a native handle to its content-surface child id (`INVALID_ID` = main window). `WindowManager` owns the mapping and supplies this (via `childIdForHandle`); event peers call it at poll time to stamp `event.childWindowId`.

### Ui::Window::NativeDisplayHandle

Header: `include/ui/window/nativedisplayhandle.h`

Lightweight per-platform display alias: `Display *` (X11), `wl_display *` (Wayland), `void *` (Win32/macOS - no display concept, always `nullptr`).

## EGL context

### Ui::Window::EglContext

Header: `include/ui/window/eglcontext.h`

`final class EglContext : public Ui::IContext, private Common::NonCopyable`. The Linux `Ui::IContext` implementation over EGL, for both X11 and Wayland. The enum `EglPlatform : unsigned char { X11, Wayland }` selects the backend. On non-Linux builds every method is a compiled-out stub.

```cpp
static std::unique_ptr<EglContext> create(NativeDisplayHandle display, EglPlatform platform);
bool init(NativeDisplayHandle display) override;                 // defaults to X11
bool init(NativeDisplayHandle display, EglPlatform platform);
bool chooseConfig(bool wantAlpha, bool wantMsaa) override;       // priority-ordered attempts
bool chooseConfigForVisual(uint64_t targetVisualId, bool wantMsaa) override;  // match an X11 32-bit ARGB visual
[[nodiscard]] uint64_t visualId() const override;               // EGL_NATIVE_VISUAL_ID (X11 only)
bool createSurface(NativeWindowHandle window, fpx_t width, fpx_t height) override;
bool createSurface(NativeWindowHandle window) override;
void resize(fpx_t width, fpx_t height) override;                // Wayland wl_egl_window only; X11 no-op
bool createContext() override;                                  // GL 3.3 compatibility profile
bool makeCurrent() override;
void swapBuffers() override;
void release() override;
void cleanup() override;                                        // teardown (see EGLDisplay lifetime rule)
[[nodiscard]] bool hasAlpha() const override;
[[nodiscard]] bool hasMsaa() const override;
[[nodiscard]] bool isValid() const override;
```

Display + config caching and ownership:

```cpp
void setOwnsDisplay(bool ownsDisplay) override;                 // only the owner terminates the shared EGLDisplay
void cacheConfig() override;                                    // cache display+config for sibling contexts
bool initCachedConfig(NativeDisplayHandle display) override;    // init from the cache (false if none yet)
```

EGLDisplay lifetime rule (important): the `EGLDisplay` is shared by every context on the same X connection - a popup reuses the main window's `Display*`, so `eglGetPlatformDisplay` hands back the same handle. Terminating it from a per-window teardown would invalidate all other live contexts. Only the connection owner (the main window, marked via `setOwnsDisplay(true)`; `m_ownsDisplay`) calls `eglTerminate` - once, in its own `cleanup()`, after its popups are already gone. A process-wide cache (`cacheConfig` / `initCachedConfig`) lets each popup skip the slow `eglInitialize` + config search; it is invalidated when the owner terminates. `chooseConfigForVisual` exists because EGL's default selection often returns a 24-bit visual even when 8-bit alpha is requested, so X11 32-bit ARGB transparency needs an explicit visual-id match.

## Events

### Ui::Window::Event / EventType / MouseButton / KeyModifier

Header: `include/ui/window/event.h`

Platform-agnostic input types.

```cpp
enum class EventType : unsigned char {
    None, CloseRequested, Resize, MouseButtonPress, MouseButtonRelease,
    MouseMove, MouseLeave, Scroll, KeyPress, KeyRelease
};
enum class MouseButton : unsigned char { Left = 1, Middle = 2, Right = 3 };
enum class KeyModifier : uint32_t { None = 0, Shift = 1<<0, Control = 1<<1, Alt = 1<<2, Meta = 1<<3 };
```

Free helpers: `toInt`/`toMouseButton`, `toUint`/`toKeyModifier`, `operator|`/`operator&` on `KeyModifier`, and `hasModifier(modifiers, flag)`.

```cpp
struct alignas(128) Event final {
    struct { int x, y; MouseButton button; int clickCount; } mouse;   // clickCount: 1 single, 2 double, ... (press only)
    struct { uint32_t keysym; KeyModifier modifiers;
             std::array<char, 8>  text;                                // UTF-8 typed char
             std::array<char, 16> name; } key;                         // logical name ("Return", "F1", "q")
    NativeWindowHandle sourceWindow  = 0;
    id_t               childWindowId = Ui::INVALID_ID;                 // INVALID_ID = main window
    struct { fpx_t deltaY; } scroll;                                   // positive = down
    struct { fpx_t width, height; } resize;
    EventType type         = EventType::None;
    bool      isPopupEvent = false;
    [[nodiscard]] std::string toShortcutString() const;               // e.g. "Ctrl+C", "F1"
};
```

`toShortcutString()` builds a shortcut string from `key.name` + modifiers (order: Ctrl, Alt, Shift); returns empty unless `type == KeyPress` and `key.name` is populated by the platform layer.

### Ui::Window::EventFactory

Header: `include/ui/window/eventfactory.h`

Compile-time factory. `NativeEvent` aliases the platform `Ui::IEventOS` impl (`Platform::Win32Event`/`MacOsEvent`/`WaylandEvent`/`X11Event`).

```cpp
static std::unique_ptr<NativeEvent> create();
```

### Ui::Window::ClickCounter

Header: `include/ui/window/clickcounter.h`

```cpp
struct alignas(32) ClickCounter final {
    void configure(uint32_t intervalMs, int distancePx);
    int  next(uint32_t timeMs, int x, int y, uint32_t button);
};
```

Multi-click burst counter for platforms whose native events carry no click count (X11, Wayland, raw Win32). `next()` returns the position of the new press in its burst (1 fresh, 2 double, 3 triple, ...); two presses belong to the same burst when they share a button, fall within `intervalMs`, and land within `distancePx`. Thresholds come from `res/input.json` via `configure()`; defaults (`400 ms`, `5 px`) make it usable before the JSON loads. macOS uses `NSEvent.clickCount` and does not need this helper. An explicit `m_hasPrior` gate avoids treating a genuine timestamp of 0 as a fresh burst.

## Content surfaces + render queue

### Ui::Window::content_surface_t

Header: `include/ui/window/contentsurface.h`

```cpp
struct alignas(128) content_surface_t final {
    CompositeTexture composite;
    Ui::IWindow *    window  = nullptr;   // geometry / visibility / native ops (non-owning)
    Ui::IRenderer *  pairing = nullptr;   // host's window+renderer Connector: frames, events, resize, apply, readPixels
    bool             isReady = true;      // false while the host is mid async-load (skip rendering)
};
```

A host-provided content surface embedded in the viewport. Both pointers are non-owning (the host owns them). `window` carries only geometry/native ops; `pairing` (an `IRenderer`, typically a host `Connector`) drives frame production and consumes events. `composite` holds the Wayland offscreen->texture cache.

### Ui::Window::ContentHit

Header: `include/ui/window/contenthit.h`

```cpp
struct alignas(16) ContentHit final {
    Ui::IRenderer * pairing = nullptr;
    int             lx = 0;
    int             ly = 0;
};
```

Hit-test result with coordinates pre-translated into the content surface's own frame (`lx`/`ly`). In XWayland composite mode the offscreen child receives main-local coords, so translating here keeps the renderer from seeing out-of-bounds x/y. Events route to `pairing`.

### Ui::Window::CompositeTexture

Header: `include/ui/window/compositetexture.h`

```cpp
struct alignas(64) CompositeTexture final {
    GLuint texture = 0; std::vector<uint8_t> pixels; int w = 0, h = 0; bool dirty = false;
    void capture(int width, int height);   // glReadPixels into pixels, marks dirty
    void clear();                          // drop pixel data, keep the texture handle
    void destroy();                        // glDeleteTextures + clear
};
```

An offscreen surface captured to a texture for main-window compositing (the Wayland XComposite path renders a surface offscreen and blits it as a texture into the main window). `capture` reads the current framebuffer; the coordinator uploads on the `dirty` flag and blits with its shared composite shader.

### Ui::Window::RenderQueue

Header: `include/ui/window/renderqueue.h`

```cpp
class RenderQueue final : private Common::NonCopyable {
public:
    void request(id_t windowId);
    [[nodiscard]] const std::set<id_t> & pending() const;
    [[nodiscard]] bool hasPending() const;
    void clear();
};
```

The set of window ids needing a redraw this frame. Windows push their id via their render-request callback (wired by the coordinator); `WindowManager::renderFrame()` reads `pending()`, clears, then repaints main + popups in the correct GL-context order. A `std::set` deduplicates and orders by id.

## Popup/dialog windows

### Ui::Window::Popup::PopupWindow

Header: `include/ui/window/popup/popupwindow.h`

`class PopupWindow : public NativeWindow`. A borderless, always-on-top popup surface (dropdowns, context menus): no decorations, above parent and children, absolutely positioned, per-corner shape. On Wayland it uses `xdg_popup` semantics; on X11 an override-redirect / transient child window with an ARGB visual where a compositor is present. It is a pure surface - the renderer pairing lives in the coordinator's `Connector`.

```cpp
explicit PopupWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID);
bool create(NativeWindow & parent, fpx_t width, fpx_t height);      // platform-agnostic entry point
void setBackground(const Ui::Color & color) override;
[[nodiscard]] Ui::Color background() const;
void clear() override;
void setRoundedCorners(bool enabled, fpx_t radius) override;
void setCornerRadii(const Ui::Res::Type::border_t & radii);         // per-corner (TL,TR,BR,BL); < 0.5 -> 0
[[nodiscard]] const Ui::Res::Type::border_t & cornerRadii() const;
[[nodiscard]] bool containsPoint(fpx_t px, fpx_t py) const;         // rounded-corner-aware hit test
[[nodiscard]] bool containsPoint(int px, int py) const;
[[nodiscard]] bool followsParent() const;
[[nodiscard]] bool hasAlpha() const;
[[nodiscard]] bool hasCompositor() const;
[[nodiscard]] bool msaaVisual() const;
[[nodiscard]] bool isWayland() const;
```

- `create(parent, width, height)` - set position and background first (`setPosition`/`setBackground`), then call this; it extracts native handles from `parent` and dispatches to the platform creator. Dimensions are physical pixels. Parent must outlive the popup. The base `Ui::IWindow::create(...)` is overridden private to force this entry point.
- `followsParent()` - true when the popup is an X11 child of the main window: the server moves and re-anchors it with the parent, so the shell must NOT manually track move/resize (doing so double-moves it).
- `containsPoint` - honors `cornerRadii` (with an anti-aliased edge band) so hits in the rounded corners fall outside the shape.
- Wayland-only extras (guarded): `wasDismissed()` (compositor dismissed the popup), `popupSurface()`.

### Ui::Window::Popup::DialogWindow

Header: `include/ui/window/popup/dialogwindow.h`

`final class DialogWindow : public PopupWindow`. A modal dialog window - owns the dialog-specific window logic (sizing from `res` dialog config, centering on the parent between the top chrome and status bar, creation). The `DialogRenderer` pairing lives in the coordinator's `Connector`.

```cpp
explicit DialogWindow(Ui::PubSub::Subscribe & subscribe, id_t subscribeId = Ui::INVALID_ID);
bool open(NativeWindow & parentWindow, const Ui::Res::ResManager & resManager,
          const Ui::Res::Type::dialog_t & dialog);                  // window only; coordinator emplaces renderer after
void recenter(const Ui::Res::Type::bound_t & parentBound, fpx_t uiTop, fpx_t uiBottom);
```

- `open(...)` - sizes from `dialog.width/height` (or the layout defaults), centers on the parent between `uiTop`/`uiBottom`, applies the theme background and scaled corner radii, and creates the window. Returns `false` on creation failure. The coordinator emplaces the renderer afterward.
- `recenter(...)` - re-center on window resize (positions snapped to integer pixels to avoid sub-pixel composite misalignment on XWayland).

## Platform peers

Headers: `include/ui/window/platform/{x11,wayland,macos,win32}window.h` and the matching `*event.h`.

Each OS provides one window impl and one event impl, selected at compile time through the `NativeWindow` / `NativeEvent` aliases - the coordinator and popups only ever name the aliases, never a concrete platform class.

- Window peers (`Platform::X11Window` / `WaylandWindow` / `MacOsWindow` / `Win32Window`) derive `WindowBase<Derived>` (CRTP) and implement the platform half of `Ui::IWindow`: native handle/display accessors, `create` / `destroy`, `resize` / `move` / `moveResize` / `show` / `hide`, `makeCurrent` / `swapBuffers` / `clear`, `screenPosition` / `screenFramePosition`, `setTitle` / `setWindowIcon`, and rounded-corner application. The X11 window owns its `EglContext` and the `m_ownsDisplay` flag that gates `eglTerminate` (see the EGLDisplay lifetime rule above); `NativeWindow::queryDpi(display)` feeds `g_config.scale`.
- Event peers (`Platform::X11Event` / `WaylandEvent` / `MacOsEvent` / `Win32Event`) implement `Ui::IEventOS`: `init(window)`, `pollEvent(event, window)` / `hasPendingEvents(window)` / `flush(window)`, `setChildWindowLookup(child_id_fn_t)` (wired to `WindowManager::childIdForHandle`), `addPopupWindow` / `removePopupWindow`, `setDoubleClickConfig(intervalMs, distancePx)`, and `copyToClipboard(window, text)`. Peers on platforms without native click counts use `ClickCounter` to stamp `event.mouse.clickCount`.

## Pub/sub

### Ui::PubSub::Subscribe

Header: `include/ui/pubsub/subscribe.h`

```cpp
class Subscribe final : private Common::NonCopyable {
public:
    void add(id_t source, id_t subscriber, Ui::task_fn_t callback);
    void add(std::initializer_list<id_t> sources, id_t subscriber, const Ui::task_fn_t & callback);
    void remove(id_t subscriber);
    void notify(id_t source);
    void defer(Ui::task_fn_t action);
    void drainDeferred();
};
```

An in-process publish/subscribe dispatcher keyed by numeric source/subscriber ids. Everything in the windowing layer wires through it: windows subscribe to content-changed sources for corner recapture, the shell subscribes to `EventBase + EventType` for policy, and windows own their subscriber id (RAII-removed in `WindowBase`'s destructor).

- `add(source, subscriber, callback)` - register `callback` to fire when `source` is notified; the `initializer_list` overload registers one callback across several sources.
- `remove(subscriber)` - drop every entry for a subscriber id (used by `WindowBase`'s destructor).
- `notify(source)` - invoke all callbacks for `source`. It iterates a snapshot, so a callback may safely add/remove entries during dispatch.
- `defer(action)` / `drainDeferred()` - queue work to run later (e.g. `WindowManager::openDialog` defers the close callback so the dialog is not destroyed mid-event); `drainDeferred()` runs the queue and re-runs anything a deferred action itself deferred.

### Ui::PubSub::SourceId / SubscriberId + helpers

Header: `include/ui/pubsub/subscribeid.h`

The single source for the numeric id-space partitioning. Element-id ranges are a UI-framework concern (`Ui::ElementId`, see [render.md](render.md)) and only documented here for the full picture.

```cpp
enum class SourceId : id_t {
    MainWindow = 1, WsBase = 100, PopupBase = 200, DockBase = 300, EventBase = 10000, ActionBase = 20000,
};
enum class SubscriberId : id_t { App = 10000 };

constexpr id_t sourceId(SourceId base);
constexpr id_t sourceId(SourceId base, id_t offset);
inline    id_t eventSourceId(Ui::Window::EventType type);   // EventBase + type
constexpr id_t wsSourceId(id_t wsId);                       // WsBase + wsId
constexpr id_t dockSourceId(id_t dockIndex);               // DockBase + dockIndex
constexpr id_t subscriberId(SubscriberId id);
```

Ranges: `1-99` window ids, `100-199` workspace/content render ids (`WsBase + id`), `200-299` popup ids (`PopupBase + id`), `300-399` dock source ids (`DockBase + index`), `10000+` OS event source ids (`EventBase + EventType`), `20000+` action source ids. On the subscriber side, `10000` is the app subscriber id.

## Usage: registering a content surface

```cpp
// A host owns its content window + renderer, pairs them in a Connector, and
// registers the pairing with the coordinator. The framework drives it blind
// through IWindow + IRenderer + IEventApp.
Ui::Window::WindowManager & wm = /* from Ui::Shell */;

// Build a host pairing: a native window bound to a UiRenderer (any Ui::IRenderer).
auto surface = std::make_unique<Ui::Window::Connector<Ui::Window::NativeWindow, Ui::Render::UiRenderer>>(
    wm.subscribe(), Ui::PubSub::wsSourceId(0));
surface->window().create(/* width */ 800, /* height */ 600, /* display */ nullptr,
                         wm.mainWindow().nativeHandle(), "viewport");
surface->emplaceRenderer(/* renderer ctor args; context is made current for you */);
surface->render();   // makes the window's context current, then draws (no swap)

// Register with the coordinator. window + pairing must outlive the registration.
const Ui::id_t surfaceId = 1;
wm.addContentSurface(surfaceId, surface->window(), *surface);
wm.setActiveContentSurface(surfaceId);
wm.setContentSurfaceReady(surfaceId, true);   // clear/set around async loads
```

## See also

[interfaces.md](interfaces.md), [render.md](render.md), [shell-actions.md](shell-actions.md), [README.md](README.md)
