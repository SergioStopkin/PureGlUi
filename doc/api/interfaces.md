# ui/interface/ - seams (Ui::)

These are the abstract contracts hosts and backends plug into. The framework
never talks to a concrete window, GL backend, or OS event source directly - it
drives `[main window] + content surfaces` blind through `IWindow` + `IRenderer` +
`IEventApp`, and executes decoded input through `IChromeCommands`. Implement one
of these to swap a backend, embed a content surface, or record behavior in a
test.

## Ui::IRender

Header: `include/ui/interface/irender.h`

The immediate draw sink. Domain-blind: the framework traverses its layout and
issues primitive draw calls in already-relocated fw value types; a backend
executes them. It never sees menus, actions, or workspaces.

Do not confuse with `IRenderer`: `IRender` is the low-level per-primitive draw
sink (fillRect / drawText / ...). `IRenderer` is a renderer OBJECT's lifecycle
contract (render / resize / cleanup / ...). See the note under `IRenderer`.

Frame protocol: `beginFrame()` once, any number of draw calls, `endFrame()`. A
backend may buffer calls (e.g. batch text by font) and flush in `endFrame()`.
Draw order is call order, except text, which a backend may defer so it
composites above non-text in the same frame.

Pure-virtual methods:
- `void beginFrame(fpx_t width, fpx_t height)` - set the viewport (physical px)
  and reset GL state.
- `void endFrame()` - flush any buffered work.
- `void fillRect(const Res::Type::bound_t &bound, const Res::Type::border_t &radii, const Res::Type::color_pair_t &colors, const Render::shadow_t &shadow)` -
  rounded-rectangle fill with per-corner radii. `colors.fg` is the fill;
  `colors.bg` is the antialiasing/compositing background. An invisible shadow
  (`{}`) is skipped.
- `void drawText(font_handle_t font, std::string_view text, const Res::Type::bound_t &pos, const Color &color, Res::Type::AlignH alignH, Res::Type::AlignV alignV, fpx_t minPadH)` -
  text run inside `pos`, placed on both axes. `AlignH` is
  `Left|Center|Right|CenterClamped` (the last centres while the label fits and
  falls back to left rather than overflowing); `AlignV` is `Top|Center|Bottom`,
  where `Center` is cap-height centred. `minPadH` is the minimum side padding.
  The backend owns baseline placement from the font handle.
- `void drawImage(std::string_view src, const Res::Type::bound_t &bound, const Res::Type::border_t &radii, const Color &tint, fpx_t scale, const Render::shadow_t &shadow)` -
  image (SVG or raster, inferred from `src`) scaled into `bound` with the given
  corner radii. `tint` with `a() > 0` recolors; `scale > 1` enlarges around the
  center (icon hover/active). An invisible shadow (`{}`) is skipped.
- `void warmImage(std::string_view src, const Res::Type::bound_t &bound, const Color &tint, fpx_t scale)` -
  pre-warm an image into the sink's cache at the given scale so a later
  `drawImage` at active scale is hitch-free. Same args as the eventual
  `drawImage` minus the radii/shadow the warm pass does not need.
- `void drawTriangle(fpx_t x0, fpx_t y0, fpx_t x1, fpx_t y1, fpx_t x2, fpx_t y2, const Color &color)` -
  flat-color triangle, vertices in CSS px. Used for the tab loading-bar arrow
  tip - the one shape that is not a rounded rect.
- `fpx_t textWidth(font_handle_t font, std::wstring_view text)` - measure text
  width. Text is wide (canonical project text type) so measurement/truncation
  stay codepoint-safe for UTF-16/UTF-32 symbols.
- `font_handle_t createFont(const Res::Type::font_t &font)` - create/resolve a
  font handle from a font resource.

Implemented by: `Ui::Gl::GlRender` (the GL backend, today the only impl).

Called by: the render-chrome layer (`Ui::Render::UiLayout` / `UiRenderer`) while
producing a frame.

## Ui::IEventApp

Header: `include/ui/interface/ieventapp.h`

The application-level pointer-event sink. Shared by windows, renderers, and the
window manager; events flow through the chain
`WindowManager -> IWindow -> IRenderer`. Self-rooted (own virtual dtor) - the fw
avoids a shared interface base.

This is the inheritance root of both `IRenderer` and `IWindow`: both derive from
`IEventApp`, so any renderer or window is also a pointer-event sink.

Every button is delivered, not just Left - a content surface needs middle for
pan and right for a context menu - so chrome implementations must ignore what
they do not handle rather than assume Left.

Press, release and scroll all report a `Render::element_event_t`
(`{type, id, event, x, y, changed}`); `changed` carries what a `bool` return
used to mean, namely "this needs a repaint". `event` is a single `EventKind`,
never a mask.

Pure-virtual methods:
- `bool onMouseMove(int x, int y)` - pointer moved; returns true if handled/dirty.
- `Render::element_event_t onMousePress(int x, int y, Window::MouseButton button, int clickCount)` -
  pointer pressed. `clickCount`: 1 = single, 2 = double, ... populated by the
  platform event layer. No default arg (prohibited on virtuals); callers that do
  not care pass 1 explicitly.
- `Render::element_event_t onMouseRelease(int x, int y, Window::MouseButton button)` - pointer released.
- `bool onMouseLeave()` - pointer left the surface.
- `Render::element_event_t onScroll(int x, int y, fpx_t deltaY)` - scroll wheel.

Implemented by: everything in the `IRenderer` and `IWindow` hierarchies
(`Ui::Render::UiRenderer`, the popup renderers, host content renderers; the
per-OS windows; `Ui::Window::WindowManager`).

Called by: `Ui::Window::WindowManager` when dispatching decoded OS events.

## Ui::IRenderer

Header: `include/ui/interface/irenderer.h`

`class IRenderer : public IEventApp` - a renderer OBJECT's lifecycle contract
(render / resize / apply / refresh / cleanup / statusText / readPixels), plus the
inherited `IEventApp` pointer events. This is the seam a host implements to embed
a content surface.

NOTE (common confusion): distinct from `Ui::IRender`. `IRender` is the low-level
draw sink (fillRect / drawText / ...) a backend implements; `IRenderer` is a
renderer object's lifecycle the window layer drives. A `UiRenderer` (an
`IRenderer`) owns a `GlRender` (an `IRender`) internally.

Methods:
- `bool render()` (pure) - render the current frame; returns true if a frame was
  rendered.
- `void resize(fpx_t width, fpx_t height)` (pure) - handle window resize.
- `void apply(Ui::Res::Type::Changed changed)` (pure) - apply resource changes
  (theme, layout, etc.).
- `void refresh()` (virtual, default no-op) - re-present the last rendered frame
  without clearing or re-drawing. Used after the parent window swaps buffers so
  this window's content stays visible (fullscreen/unredirected compositing). A
  content-surface renderer may override it.
- `void cleanup()` (pure) - release resources.
- `const std::string &statusText() const` (virtual, default empty string) -
  status text, e.g. content stats for a content-surface renderer.
- `std::vector<uint8_t> readPixels(int &outWidth, int &outHeight)` (virtual,
  default empty) - read the rendered surface as RGBA pixels (top-left origin) so
  the window layer can snapshot any content surface for compositing without
  knowing the renderer type. UI chrome and popups (never composited) need not
  implement it; a content-surface renderer overrides it with a framebuffer read.

Implemented by: `Ui::Render::UiRenderer` (the UI chrome), the popup renderers
`Ui::Render::Popup::PopupRenderer` / `DialogRenderer` (via
`PopupRendererBase`), and host content renderers (which override `readPixels` /
`statusText` / `refresh`).

Called by: the window/coordinator layer (`Ui::Window::WindowManager` and the
window pairing glue) to produce frames and react to resize/resource changes.

## Ui::IChromeCommands

Header: `include/ui/interface/ichromecommands.h`

The intents-in execution seam: the exact vocabulary of "what an intent does". It
is the counterpart to the intents-out `Ui::Render::Context` (which maps
clicks/keys into `intent_t`). Input decoding and input execution are thereby
split into two seams that meet only through `intent_t`.

Pure-virtual methods:
- `void emitAction(const std::string &actionKey, const std::string &arg)` -
  dispatch a data-driven action (`arg` is the item label/value for
  parameterized actions, `""` otherwise).
- `bool isPopupOpen() const` - gates the ClosePopup path (a stray close with no
  open popup is a no-op).
- `void openPopup(id_t menuId)` - open the popup menu for `menuId`.
- `void closePopup()` - close the open popup.
- `void openDialog(id_t itemId)` - open the modal dialog carried by a menu item
  (resolved by item id).
- `void switchTab(id_t tabId)` - switch active tab (host-domain reaction behind
  the shell's tab hook).
- `void closeTab(id_t tabId)` - close a tab (host-domain reaction).
- `void copyText(const std::string &text)` - copy text to the clipboard
  (status-bar text click).
- `void activateRow(id_t rowId)` - a dock row was selected (host-domain
  reaction: the row tree is the host's model).
- `void toggleRow(id_t rowId)` - a dock row's expand/collapse was requested. The
  host flips its own flag and re-projects via `WindowManager::setDockRows`; the
  framework holds no expanded state of its own.

Free function (same header):
- `inline void routeIntent(const intent_t &intent, IChromeCommands &chrome)` -
  the pure `IntentKind` -> command switch, and the only caller of the interface.
  Stateless, so the app and tests share one copy. `OpenSubmenu` is a no-op on the
  click path (submenus open on hover); `ClosePopup` is gated by `isPopupOpen()`;
  `CopyText` is skipped on empty `arg`.

Implemented by: `Ui::Shell` (privately - forwarding to `WindowManager` + the
action registry + host tab hooks), and by test recording doubles.

Called by: `Ui::routeIntent()` only, invoked from `Ui::Shell::execute()`.

## Ui::IWindow

Header: `include/ui/interface/iwindow.h`

`class IWindow : ... ` derives from `IEventApp` (via the base rooting - a window
is also a pointer-event sink). A pure presentation surface: GL context +
geometry + visibility. It never renders or consumes events itself; a pairing
(Connector) glues it to a renderer.

Pure-virtual methods:
- `void setPosition(fpx_t x, fpx_t y)` - pre-creation position hint.
- `bool create(fpx_t width, fpx_t height, Ui::Window::NativeDisplayHandle display, Ui::Window::NativeWindowHandle parentWindow, const std::string &title)` -
  create the surface. Optional title and platform-specific handles (display,
  parent); pass nullptr for unused on other platforms.
- `bool isValid() const` - surface created and usable.
- `Ui::Window::NativeWindowHandle nativeHandle() const` - native window handle.
- `Ui::Window::NativeDisplayHandle nativeDisplay() const` - native display handle.
- `void destroy()` - destroy the surface.
- `void setTitle(const std::string &title)` - set window title.
- `void screenPosition(int &screenX, int &screenY) const` - client-area top-left
  in screen coords (use for popup placement, anchored to the client area).
- `void resize(fpx_t width, fpx_t height)` - resize.
- `void move(fpx_t x, fpx_t y)` - reposition.
- `void moveResize(const Ui::Res::Type::bound_t &bound)` - move + resize in one.
- `void show()` / `void hide()` - visibility.
- `void setBackground(const Ui::Color &color)` - background color.
- `void setRoundedCorners(bool enabled, fpx_t radius)` - rounded window corners.
- `Ui::Res::Type::bound_t bound() const` - current geometry.
- `bool isHardwareGl() const` - true if the GL context runs on a real GPU; false
  only on a software/CPU rasterizer fallback (e.g. macOS without GPU
  passthrough). Consumers gate features that stall on software GL (IBL prefilter,
  MSAA, ...).
- `void makeCurrent()` - make this window's GL context current.
- `void swapBuffers()` - swap front/back buffers.
- `void clear()` - clear framebuffer with the background color.
- `void requestRender()` - queue a render via the render-request callback (the
  coordinator produces the actual frame).
- `void setRenderRequest(Ui::task_fn_t fn)` - wire the render-request callback
  (the window layer points this at its render queue).

Virtual (default provided):
- `void screenFramePosition(int &screenX, int &screenY) const` (default delegates
  to `screenPosition`) - screen position of the outer (decorated) frame. Use this
  for session persistence: `screenPosition` (client area) does not round-trip
  through `moveResize` on reparenting WMs and drifts the window down by the frame
  top each cycle. Platforms without server-managed decorations (e.g. Wayland)
  inherit the delegating default.

Implemented by: the per-OS windows (`Ui::Window::WindowBase`-based
`platform/{x11,wayland,macos,win32}window.h`) and the popup windows
`Ui::Window::Popup::PopupWindow` / `DialogWindow`.

Called by: `Ui::Window::WindowManager` (the coordinator) and the window pairing
glue.

## Ui::IEventOS

Header: `include/ui/interface/ieventos.h`

OS-level event polling + clipboard. Platform-agnostic; does NOT own windows -
receives them as parameters. Self-rooted (own virtual dtor).

Pure-virtual methods:
- `void init(IWindow &window)` - initialize from the main window (extract
  platform-specific data).
- `void setChildWindowLookup(Ui::Window::child_id_fn_t lookup)` - set the
  child-window (content-surface) lookup: native handle -> child id.
  `WindowManager` owns the content-surface registry and supplies this; the event
  peer calls it at poll time to stamp `event.childWindowId`. Unset (or a lookup
  returning `INVALID_ID`) means the event belongs to the main window.
- `void addPopupWindow(Ui::Window::NativeWindowHandle handle)` - register a
  popup-type window (menu/submenu/dialog) for event routing.
- `void removePopupWindow(Ui::Window::NativeWindowHandle handle)` - unregister a
  popup-type window.
- `bool hasPendingEvents(IWindow &window) const` - true if events are waiting.
- `bool pollEvent(Ui::Window::Event &event, IWindow &window)` - non-blocking:
  fill `event`, returns false if none pending.
- `void flush(IWindow &window)` - flush pending output to the display server.
- `bool copyToClipboard(IWindow &window, const std::string &text)` - copy text to
  the system clipboard (window used for selection ownership).

Virtual (default no-op):
- `void setDoubleClickConfig(uint32_t intervalMs, int distancePx)` - configure
  double-click thresholds (loaded from `res/input.json` by `ResManager`). No-op
  where the OS supplies a native click count (e.g. macOS `NSEvent.clickCount`).

Implemented by: the per-OS event peers (`platform/*event.h`: `X11Event`,
`WaylandEvent`, `MacOsEvent`, `Win32Event`).

Called by: `Ui::Window::WindowManager` in the event loop.

## Ui::IContext

Header: `include/ui/interface/icontext.h`

Graphics-context abstraction (OpenGL via EGL here). Wraps platform-specific
context creation and management. Self-rooted (own virtual dtor).

Pure-virtual methods:
- `bool init(Ui::Window::NativeDisplayHandle display)` - initialize for a display.
- `bool chooseConfig(bool wantAlpha, bool wantMsaa)` - choose a suitable
  config/visual (`wantAlpha` = transparency, `wantMsaa` = multisampling).
- `bool chooseConfigForVisual(uint64_t visualId, bool wantMsaa)` - choose a
  config matching a specific native visual id.
- `void setOwnsDisplay(bool ownsDisplay)` - mark whether this context owns the
  display connection and is thus responsible for terminating the shared display
  on teardown. Only the main window (which opened the connection) owns it; popups
  reuse the parent's connection and must not terminate it.
- `void cacheConfig()` - cache the current display+config so a sibling context
  can reuse them.
- `bool initCachedConfig(Ui::Window::NativeDisplayHandle display)` - initialize
  from a previously cached display+config, skipping the slow init/search; false
  if nothing is cached yet.
- `uint64_t visualId() const` - native visual id for window creation.
- `bool createSurface(Ui::Window::NativeWindowHandle window)` - create a surface
  for a window.
- `bool createSurface(Ui::Window::NativeWindowHandle window, fpx_t width, fpx_t height)` -
  create a surface with explicit size (required for Wayland).
- `bool createContext()` - create the OpenGL context.
- `bool makeCurrent()` - make this context current for rendering.
- `void swapBuffers()` - swap front/back buffers.
- `void resize(fpx_t width, fpx_t height)` - resize the drawable (no-op where the
  surface tracks the window).
- `void release()` - release the context (make none current).
- `void cleanup()` - clean up all resources.
- `bool hasAlpha() const` - context has an alpha channel.
- `bool hasMsaa() const` - context has MSAA.
- `bool isValid() const` - context is valid and ready.

Implemented by: `Ui::Window::EglContext`.

Called by: the per-OS windows (`WindowBase`-based windows own an `IContext`) and,
indirectly, `Ui::Window::WindowManager` during window creation/teardown.

## See also

- [render.md](render.md)
- [windowing.md](windowing.md)
- [shell-actions.md](shell-actions.md)
- [README.md](README.md)
