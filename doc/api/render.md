# ui/render + ui/gl - chrome renderer and GL backend

The render-chrome layer (`Ui::Render`, `Ui::Render::Popup`) is the domain-blind UI chrome: it lays out the fixed main-window regions, popups, and dialogs, and draws them through the abstract draw sink `Ui::IRender`. The GL backend (`Ui::Gl`) is the bottom concrete `IRender` implementation (`GlRender`) plus its building blocks (SDF rounded rects, SVG textures, FreeType text).

Layering, top to bottom:

- `Ui::Render::UiRenderer` and the `Ui::Render::Popup` renderers implement `Ui::IRenderer` (renderer-object lifecycle: render/resize/apply/cleanup - see [interfaces.md](interfaces.md)) and are self-rooted `Ui::IEventApp`s (pointer-event sinks).
- They emit primitives through `Ui::IRender` (fillRect/drawText/drawImage/drawTriangle/measure/createFont). `UiRenderer` holds the sink as an injected `std::unique_ptr<Ui::IRender>`, so it is decoupled from GL: `Ui::Gl::GlRender` in the app, a fake sink in headless tests.
- `Ui::Render::Context` never draws; it maps clicks/keys to `Ui::intent_t` (intents-out; see [vocabulary.md](vocabulary.md)).

Interaction is capability-driven rather than type-driven. An element declares which events it accepts (`EventKind`, a bitmask) and a `(type, event)` table says what each raises (`binding_t`) and what must repaint afterwards (`RenderScope`). Adding an interactive element is therefore declaring a mask and a row, not editing hit-test, hover and dispatch switches - see [Input model](#input-model-uirender).

House rule for rounded shapes: every rounded rectangle, pill, split-fill, or corner-rounded surface MUST go through `Ui::Gl::Rounded`. Do not write a parallel SDF/scissor/stencil path anywhere else - if `Rounded` lacks a mode you need, extend `Rounded`. The only exception is a host content surface, which renders its own content.

All chrome geometry is in CSS pixels; the GL backend scales to physical pixels via `Ui::g_config.scale`. Header paths below are relative to `include/`.

---

## Render chrome (Ui::Render)

### UiElementType

`ui/render/uielement.h` - `enum class UiElementType : unsigned char`. The kinds of element `UiLayout` produces: `MenuButton`, `ToolbarButton`, `MenuItem`, `Separator`, `Text`, `Image`, `Tab`, `TabClose`, `TabArrow`, plus the dock elements `DockRow`, `DockExpander`, `DockGrip`, `DockSlider`, `DockScrollbar`. Free helper `int toInt(UiElementType)`.

Also here: `constexpr EventKind defaultAccepts(UiElementType)` - the event mask a fresh element of this type is stamped with by `UiLayout::addElement`. A caller needing something different overrides `accepts` on the returned element, which is how an element stops being a switch case. The values reproduce behaviour that used to be spread across `UiLayout::hitTest`, `UiRenderer::updateHover` and `PopupRenderer::onMousePress`; `Text` is the notable one - clickable (the status bar copies itself) but deliberately not hoverable, which used to be a `type != Text` test. `DockGrip`, `DockSlider` and `DockScrollbar` are `ANY_DRAG` (drag-only); `Separator` and `Image` accept nothing.

### UiElementState

`ui/render/uielementstate.h` - `enum class UiElementState : unsigned char { None = 0, Hovered, Active, Disabled }`. Interaction state of a laid-out element. `Disabled` elements are inert (skipped by hit-test and hover). Free helper `int toInt(UiElementState)`.

### UiElement

`ui/render/uilayout.h` - `struct alignas(32) UiElement final`. One laid-out element:

- `UiElementType type` (default `MenuButton`)
- `UiElementState state` (default `None`)
- `id_t id` (default `INVALID_ID`)
- `Ui::Res::Type::bound_t bound`
- `EventKind accepts` (default `None`) - the capability mask; anything outside it never reaches this element

Content (text, icons, shortcuts) and appearance (colors, fonts) are NOT stored here - renderers resolve them from `ResManager` at render time by `type` + `id`.

### UiLayout

`ui/render/uilayout.h` - `class UiLayout final`. Lightweight layout engine: turns `layout_t`/`theme_t`/menu/button/tab data into a flat `std::vector<UiElement>` for direct rendering. Coordinates are CSS pixels. The tab-arrow and status-text ids it assigns come from `ui/elementid.h`.

Methods:

- `void build(const layout_t &, const theme_t &, const std::vector<menu_t> &, const std::vector<button_t> &, const TabBar &, const LocaleManager &, fpx_t windowCssW, fpx_t windowCssH, Ui::IRender * render, const std::string & statusText = "")` - build the main-window layout: top-menu buttons (left-aligned label menus, right-aligned square icon/action menus), left toolbar buttons, status-bar text, and workspace tabs. Resolves and caches the font handles via `render->createFont`. `render` may be null (measurement falls back to `layout.fallbackCharWidth`).
- `static std::vector<UiElement> buildPopup(const menu_t & menu, const ResManager &)` - build popup dropdown items (`MenuItem` rows + `Separator`s) from menu data.
- `UiElement * hitTest(fpx_t cssX, fpx_t cssY, EventKind event)` - topmost non-disabled element at CSS coords that ACCEPTS `event`, or `nullptr`. Iterates in reverse (last drawn wins). Passing the kind is what lets a hover pass and a click pass disagree about which element is under the same pixel - a `Text` status bar answers a click but not a hover.
- `UiElement * elementById(id_t)` - first element with matching id, or `nullptr`.
- `const UiElement * parentOf(const UiElement & child) const` - the element sharing the child's id but with a different type (e.g. a `Tab` for a `TabClose`), or `nullptr`.
- `const std::vector<UiElement> & elements() const` / `std::vector<UiElement> & elements()` - the flat draw list (draw in order).
- Cached font-handle getters: `menuFont()`, `itemFont()`, `itemFontBold()`, `popupFont()`, `statusBarFont()` (all `Ui::font_handle_t`).
- `int buttonImgSize() const` - toolbar-button image size (CSS px).
- `UiElement & addElement(UiElementType type, const bound_t & bound, id_t id = INVALID_ID)` - append an element, stamping `accepts` from `defaultAccepts(type)`. Override `accepts` on the returned reference for the exceptions (a submenu parent takes `Hover` only). Insertion order is z-order, so a contributor added after `build()` - a dock, via the extra-ops hook - sits on top of the chrome it overlaps.

Text fitting is not a `UiLayout` member: `truncateText` / `truncateFileName` are free functions in `ui/render/truncate.h` - see [Text fitting](#text-fitting-uirender).

Tab layout is browser-like adaptive: all tabs at max width if they fit; else shrink to min width; else show scroll arrows over a visible subset.

### UiRenderer

`ui/render/uirenderer.h` - `class UiRenderer final : public Ui::IRenderer, private Common::NonCopyable`. The main-window renderer. Walks a `UiLayout`, produces batched draw ops, and submits them through the injected `Ui::IRender`. Window-free: no `IWindow` reference, just a cached physical size and (through the sink) a `makeCurrent` capability. Compile-time `constexpr bool UI_DEBUG = false`.

Constructor:

- `UiRenderer(std::unique_ptr<Ui::IRender> render, fpx_t width, fpx_t height, const Ui::Res::ResManager & resManager)` - `render` is the injected draw sink (GL in the app, fake in tests); `width`/`height` are the initial physical size.

Public op struct types (batched draw ops, all `alignas`): `BGOp` (rect + radius + optional shadow + `color_pair_t`), `TextOp` (text/font/color/pos/centered/minPadH), `ImageOp` (src/pos/radius/tint + isSvg/tinted/hovered/active flags), `progress_t` (tab loading-bar geometry).

`Ui::IRenderer` overrides:

- `bool render()` - render at the cached size (delegates to `Render`).
- `void resize(fpx_t w, fpx_t h)` - cache the new physical size; layout rebuilds on the next `setContent()`.
- `void cleanup()` - reset the layout.
- `void apply(Changed)` - no-op (content rebuild is driven externally via `setContent()` to avoid duplicate SVG loads).

Other methods:

- `void setContent()` - rebuild the entire `UiLayout` from current `ResManager` state, then re-apply hover/active state.
- `const Ui::Res::Type::bound_t & bound(id_t)` - element bounds by id (empty bound if not found).
- `void Render(fpx_t width, fpx_t height)` - render a frame at the given physical size: clears op buffers, generates ops from the layout, runs the extra-ops hook, then `beginFrame`/flush/`endFrame` on the sink.
- `void setExtraOpsHook(std::function<void(UiRenderer &)>)` - register a hook that runs after layout ops and before GL submission; used to draw dock columns into the same frame.
- `void appendBg(const bound_t & rect, const color_pair_t & elementColors, const Ui::Color & parentBg, const border_t & radius = {})` - op-emitter for hook users: a colored (optionally rounded) rect, no shadow. `elementColors` is the natural theme block `{fg, bg}`; `parentBg` is the surrounding fill used for edge anti-aliasing.
- `void appendImage(const bound_t & rect, const std::string & svgPath, Ui::Color tint)` - op-emitter for hook users: a tinted SVG in a rect.
- `void setOnElementHover(std::function<void(UiElementType, id_t)>)` - hover callback for menu buttons (fires by position so a disabled menu under the cursor is still reported).

`Ui::IEventApp` overrides: `bool onMouseMove(int,int)`, `element_event_t onMousePress(int,int,MouseButton,int clickCount)`, `element_event_t onMouseRelease(int,int,MouseButton)`, `bool onMouseLeave()`, `element_event_t onScroll(int,int,fpx_t)` (no-op, returns `{}`). All translate physical input to CSS via `toCss`, update element `state`, and report whether a redraw is needed via `changed`. Press and release are Left-only here; the other buttons belong to content surfaces. Hit-testing is capability-driven - `UiLayout::hitTest(x, y, EventKind)` only returns elements whose `accepts` mask includes that kind.

Internally, `generateDrawOps()` resolves colors/fonts/text/image-src per element from `ResManager` (`resolveColors`/`resolveFont`/`resolveText`/`resolveImageSrc`), and `flushOps()` translates the staged op buffers into `IRender` primitive calls in the order backgrounds -> progress -> images -> text.

### Context

`ui/render/context.h` - `class Context final : private Common::NonCopyable`. The intents-out facade. It interprets resolved main-UI input against `ResManager` data and returns intents; it never runs an action, opens a window, or mutates state.

- `explicit Context(const Ui::Res::ResManager &)`
- `Ui::result_t mapClick(const element_event_t & click, id_t openMenuId) const` - map a resolved event to intents. Most types resolve through `defaultBinding` (the `(type, event)` table): `Tab` -> `SwitchTab`, `TabClose` -> `CloseTab`, `ToolbarButton` -> `EmitAction`, `Text` -> `CopyText`, `DockRow` -> `ActivateRow`, `DockExpander` -> `ToggleRow`. `MenuButton` and `MenuItem` are resolved by `Context` itself and are deliberately absent from the table: their intent comes from res data, not from their type - a menu button with an actionKey emits it while one without toggles its popup against `openMenuId`, and a menu item is inert / opens a dialog / emits an action depending on its children and dialog block.
- `RenderScope scopeFor(const element_event_t & click) const` - the repaint class the same event implies, so a caller declares what to redraw instead of remembering which refresh call to make.
- `Ui::result_t mapKey(const std::string & normalizedKey) const` - resolve a normalized keyboard shortcut against `shortcuts.json` to an `EmitAction` intent (empty result if unbound).

See [vocabulary.md](vocabulary.md) for `intent_t`, `IntentKind`, and `result_t`.

### DockColumn

`ui/render/dockcolumn.h` - `class DockColumn final : private Common::NonCopyable`. One collapsible dock column anchored to the left or right edge. Owns the live width + expanded state for one dock and reads/writes it through `ResManager` so session persistence is transparent. Compile-time `constexpr bool DOCK_DEBUG = false`.

- `DockColumn(id_t id, const Ui::Res::Dock::dock_config_t & cfg, const Ui::Res::ResManager &)` - seeds `memoryX` from `cfg.defaultWidth` on first use.
- Accessors: `id()`, `name()`, `anchor()` (`DockAnchor`), `order()`, `currentWidth()` (content + grip, transient width while dragging), `isDragging()`, `isHoveredGrip()`, `outer()`, `content()`, `grip()` (all bounds in CSS px).
- Clamping: `clampTransientWidth(fpx_t maxExpanded)`, `clampStateWidth(fpx_t maxExpanded)` (also clamps `memoryX`, persists if changed).
- Mouse routing (CSS coords, return whether visual state changed): `bool isOverGrip(fpx_t,fpx_t) const`, `bool onMouseMove(fpx_t,fpx_t)`, `void beginGripGesture(fpx_t cssX, int clickCount)` (double-click on the grip toggles collapse/restore; single starts a drag), `void onMouseUp(fpx_t cssX)`, `void onMouseLeave()`. Capture decisions live one layer up (WindowManager).
- `void setLayout(fpx_t y, fpx_t h, fpx_t innerEdgeX)` - position the dock; `innerEdgeX` is the viewport-facing edge.
- `void render(UiRenderer & out) const` - emit content-background, grip-strip, grip-glyph, row and scrollbar ops via the renderer's extra-ops hook. Everything goes through `appendBg`/`appendImage`; only the viewport-facing corners are rounded (`gripRadius`), and the glyph is the `--dock-grip-icon` SVG tinted with the grip color.

Rows (host-projected content; see `Ui::Res::Dock::row_t` in [resources.md](resources.md)):

- `void setRows(std::vector<Ui::Res::Dock::row_t> rows)` / `const std::vector<row_t> & rows() const` - replace or read the projected content. The dock renders and hit-tests these and never learns what they mean.
- `void appendElements(UiLayout & layout) const` / `void appendRowElements(UiLayout & layout) const` - contribute `DockGrip`, `DockRow`, `DockExpander`, `DockSlider` and `DockScrollbar` elements to the main-window layout, so dock content is hit-tested by the same pass as the chrome.
- Sliders: `bool hasSliderRow(id_t rowId) const` (which dock owns a pressed slider - the element carries the row, not the dock), `std::optional<fpx_t> beginSliderGesture(id_t rowId, fpx_t cssX)` (grab the thumb in place, or jump it centred under the pointer and return the new ratio), `fpx_t onSliderDrag(fpx_t cssX) const`, `void endSliderDrag()`. The dock draws, hover-tests and drags the thumb from one geometry; `WindowManager` only routes the capture and hands each ratio to `setOnRowValue`.
- Scrolling: `bool onScroll(fpx_t deltaY)`, `bool isScrollable() const`, `void beginScrollGesture(fpx_t cssY)`, `bool onScrollDrag(fpx_t cssY)`, `void endScrollDrag()`, `bool isDraggingScroll() const`, `void resetScroll()`, `std::size_t firstRow() const`. Scroll position is a ROW INDEX, not a pixel offset - the dialog scrollbar keeps pixels, and `ScrollBar` holds neither (see [Thumb widgets](#thumb-widgets-uirender)).

A dock is not an OS surface - it appends ops into the main window's frame - which is why `RenderScope` has no per-dock value.

---

## Input model (Ui::Render)

What replaces asking "what type is this element?" in four separate switches. An element declares the events it accepts; a table says what an accepted event raises and what must repaint. A new interactive element declares a mask and a row instead of editing dispatch.

### EventKind

`ui/render/eventkind.h` - `enum class EventKind : uint16_t`. What can happen to an element, and simultaneously the per-element capability mask: `None`, `LeftClick`, `RightClick`, `MiddleClick`, `DoubleClick`, `Hover`, `DragStart` (press that begins a capture), `Drag` (motion while captured), `DragEnd` (release that ends one), `Scroll`.

Combine and test with `Common::Bit` exactly as `Res::Type::Changed` does - no bitwise operators are defined for it. Two composites are provided so a mask need not spell out constants: `ANY_CLICK` (all four click flavours) and `ANY_DRAG` (the three drag phases). `constexpr bool acceptsEvent(EventKind mask, EventKind event)` is the test.

### element_event_t

`ui/render/elementevent.h` - `struct alignas(32) element_event_t final`. What happened, to which element: `{UiElementType type, id_t id, EventKind event, fpx_t x, fpx_t y, bool changed}` plus `bool isHit() const`.

- `event` is a single kind, never a mask - `EventKind` doubles as the accept mask, so it reads like one.
- `x`/`y` are SURFACE-local CSS pixels: whatever space the producing renderer was handed, main window or popup. Subtract the element bound for element-local.
- `changed` is independent of the hit and carries what a `bool` return used to mean: clearing a stale hover with nothing under the cursor is still a repaint.

This is the return type of `IEventApp::onMousePress` / `onMouseRelease` / `onScroll` (see [interfaces.md](interfaces.md)); it replaced `click_result_t`.

### binding_t + defaultBinding

`ui/render/binding.h` - `struct alignas(8) binding_t final { IntentKind intent; RenderScope scope; }`. What an element raises when an event it accepts occurs, and what must repaint afterwards. The payload (id, actionKey, arg) is assembled by the caller, the only party that needs to know the intent's shape.

`bool defaultBinding(UiElementType type, EventKind event, binding_t & outBinding)` is the table; `false` means this type does not bind that event, so nothing happens. Today it is left-click-only: `Tab`/`TabClose` -> `SwitchTab`/`CloseTab` (`Layout`), `ToolbarButton` -> `EmitAction` (`Chrome`), `Text` -> `CopyText` (`None` - the temp status that follows is what actually repaints), `DockRow` -> `ActivateRow`, `DockExpander` -> `ToggleRow` (both `Chrome`).

`MenuButton` and `MenuItem` are deliberately absent: their intent is chosen from res data rather than from their type, which a `(type, event)` table cannot express, so `Context` resolves those two itself. `TabArrow` and `DockGrip` bind nothing because scrolling a tab bar and resizing a dock are framework-internal work, not intents. `DockSlider` binds nothing either - its value reaches the host through `WindowManager::setOnRowValue`, because a drag is a continuous stream rather than a discrete request - and `DockScrollbar` never leaves the framework at all.

### RenderScope

`ui/render/renderscope.h` - `enum class RenderScope : uint8_t { None, Chrome, Layout }`. How much must be redrawn after a binding fires: nothing; re-emit ops against the existing layout (`requestMainRender()`); or rebuild the layout first (`requestContentRefresh()`). A host content surface has no value: only the host knows its content changed, so it calls `requestRender(surfaceId)` itself.

Deliberately coarse, and named for what the framework can actually do: the smallest addressable unit is one OS surface. A dock is not a surface, so there is no per-dock scope - pretending otherwise would be a lie in the type system.

### element_style_t

`ui/render/elementstyle.h` - `struct alignas(64) element_style_t final`. Everything one element contributes to a frame: `{color_pair_t colors, std::string text, std::string imageSrc, font_handle_t font, fpx_t padH}`. Resolved in one pass so the tab/menu/button lookups happen once instead of once per attribute; an element that draws nothing leaves it default-constructed (`font == 0` means it draws no text). `text` is already truncated to the bound.

---

## Thumb widgets (Ui::Render)

A thumb on a track, factored so that the half that DRAWS it and the half that turns a pointer into a value cannot disagree about how far it travels. When those two live apart they disagree about the travel and the thumb stops sitting under the pointer - which is exactly the bug this shape exists to prevent.

Shared by three widgets with three different value types: the dialog scrollbar (an animated pixel offset), the dock scrollbar (a row index), and the dock slider (a 0..1 ratio). The VALUE therefore stays with the caller, and so does drawing.

### Axis

`ui/render/axis.h` - `enum class Axis : unsigned char { Vertical, Horizontal }`. Which way a one-dimensional widget runs. The geometry is identical either way - only which half of a `bound_t` is "along" it changes - so the axis is a parameter rather than two copies of the maths. (This header replaced `clickresult.h`, whose type is now `element_event_t`.)

### drag_track_t

`ui/render/dragtrack.h` - `struct alignas(16) drag_track_t final`. The state a one-axis drag remembers between press and release: `{fpx_t grabPos, fpx_t valueAtGrab, fpx_t scale}` (scale = value units per pixel; negative mirrors the axis).

- `void begin(fpx_t pos, fpx_t value, fpx_t unitsPerPixel)`
- `fpx_t valueAt(fpx_t pos) const` - `valueAtGrab + (pos - grabPos) * scale`
- `fpx_t travelFrom(fpx_t pos) const` - unsigned pointer travel, for the "was this a click or a drag" gate

Every drag in the framework is this - a dock grip resizing a column, a thumb sliding an offset - differing only in scale and in what the caller does with the result. Deliberately unclamped: a dock's ceiling depends on live viewport space and a thumb's on content height, and neither is knowable here.

### thumb_metrics_t

`ui/render/thumbmetrics.h` - `struct alignas(32) thumb_metrics_t final`. What a thumb is derived from, as one parameter rather than four repeated at every call: `{bound_t track, fpx_t thumbLength, fpx_t maxValue, Axis axis}`.

- `fpx_t trackStart() const` / `fpx_t trackLength() const` - the axis-relevant half of `track`
- `fpx_t travel() const` - how far the thumb's leading edge can move: `trackLength - thumbLength`, floored at zero. Shorter than the track because the thumb is kept fully inside the ends, which is what a pointer mapping has to agree with.
- `bound_t boundAt(fpx_t start) const` - the thumb from its leading edge; cross-axis it fills the track
- `fpx_t along(fpx_t cssX, fpx_t cssY) const` - the pointer coordinate that matters, so a caller holding both stays axis-agnostic

It says LENGTH and MAX rather than content and viewport on purpose: those are a scrollbar's way of arriving at them, and a slider arrives differently. Keeping the derivation with the caller is what lets both share every line.

### thumb_track_t

`ui/render/thumbtrack.h` - `struct alignas(16) thumb_track_t final`. Geometry, hover and drag: `{drag_track_t grab, bool isHoveredTrack, bool isHoveredThumb, bool isDragging}`.

- `static bound_t thumbBound(const thumb_metrics_t &, fpx_t value)`
- `static fpx_t centredValue(const thumb_metrics_t &, fpx_t pos)` - the value putting the thumb's MIDDLE at `pos`; what a press on bare track means for a widget that jumps to the pointer rather than paging
- `static bool isOnThumb(const thumb_metrics_t &, fpx_t value, fpx_t pos)`
- `void begin(const thumb_metrics_t &, fpx_t pos, fpx_t value)` - thumb travel is shorter than value travel, so this derives the units-per-pixel scale rather than taking one
- `fpx_t valueAt(const thumb_metrics_t &, fpx_t pos) const` - clamped to `[0, maxValue]`
- `bool updateHover(const thumb_metrics_t &, fpx_t value, fpx_t cssX, fpx_t cssY)` - whether either flag moved, so a caller can skip a repaint
- `void clear()`

### ScrollBar

`ui/render/scrollbar.h` - the vertical scrollbar every scrollable surface uses (the dialog and the docks). It owns the state (hover, grab) and every decision the `scrollbar` res block drives - which width, offset, radius and minimum thumb the current state takes.

It does NOT draw: the dialog paints through `Rounded`'s split-corner path, a dock emits ops through `UiRenderer`. Nor does it own the value. Hit geometry is always the HOVER width while drawn geometry is the current state's - otherwise the pointer falls out of the bar the moment its own hover widens it.

### sliderMetrics

`ui/render/slider.h` - `thumb_metrics_t sliderMetrics(const bound_t & track, fpx_t thumbLength)`. The dock slider's geometry: a FIXED thumb length over a plain 0..1 ratio (`maxValue == 1.0F`, `Axis::Horizontal`), because unlike a scrollbar there is no content behind it to be proportional to.

The length is passed rather than derived from the track because res states it per pointer state. Callers pass the HOVER length in both states - the same rule the scrollbar follows for its own widening: the travel and the hit area must be the larger of the two, or the pointer falls out of the grip at the moment the grip grows to meet it.

---

## Text fitting (Ui::Render)

`ui/render/truncate.h` - two free functions. Nothing here holds state or knows what is being drawn: the caller supplies the measuring sink, so they belong to no layer and chrome, docks and popups all reach the same two.

- `std::string truncateText(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)` - plain end-truncation, prefix + ellipsis, no filename logic. What a caller wants for text that is not a file name: "0.085 mm2" would otherwise be split into a name and an "extension" at the decimal point.
- `std::string truncateFileName(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)` - a degradation ladder: full text -> prefix + ellipsis + extension (preferred, keeps the file-type cue) -> prefix + ellipsis -> ellipsis alone -> empty. A leading dot stays in the name, so `.hidden` is not all extension.

Both are built from two helpers in the same header: `fittingPrefix(wtext, maxW, reservedW, render, font)` (the longest prefix that leaves `reservedW` free, measured over a `wstring_view` with no allocation per codepoint) and `ellipsize(wtext, maxW, render, font)` (prefix + ellipsis -> ellipsis alone -> empty). `truncateText` is `ellipsize` after the fit check; `truncateFileName` tries the extension step through `fittingPrefix` and then falls back to `ellipsize` on the name.

Both work on `std::wstring` internally so `substr()` walks by codepoint rather than byte - a UTF-8 `std::string` would slice a multi-byte sequence in half and pass invalid bytes to `textWidth()`. Conversion happens at the boundary only. Both return the input unchanged when measurement is unavailable (null sink or `font == 0`), so a caller never silently loses data.

The ellipsis is a single horizontal-three-dot glyph via `Ui::Codepoint::ThreeDotH` (~3-5 CSS px) rather than three ASCII dots (~8-12), which is what makes the tight end of the ladder land on something useful.

---

## Popup chrome (Ui::Render::Popup)

### PopupRendererBase

`ui/render/popup/popuprendererbase.h` - `class PopupRendererBase : public Ui::IRenderer, private Common::NonCopyable`. Shared base for popup-type renderers (menu popups, dialogs). Provides common GL setup, text drawing, SVG icon drawing, software-rounded corner management, and `Ui::IRenderer` boilerplate. Owns its GL building blocks by value: a `FontRenderer`, a `SvgRenderer`, and a `Rounded`. Not decoupled from GL (unlike `UiRenderer`) - these renderers issue GL directly.

- `PopupRendererBase(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager &)` - `makeCurrent` (a `Ui::task_fn_t`) makes this renderer's window GL context current; passed to the owned `FontRenderer` for lazy glyph upload and teardown.
- Destructor makes the context current so the owned GL objects are deleted in the right context (contexts are not shared).

Software-rounded corner support (popup/dialog corners composited over parent pixels):

- `void setAlpha(bool hasAlpha)`
- `void setCornerPixels(std::array<std::vector<uint8_t>,4> pixels, const Ui::Res::Type::border_t & radii)`
- `bool hasCornerPixels() const`

`Ui::IRenderer` boilerplate: `void resize(fpx_t,fpx_t)` (caches size), `void apply(Changed)` (no-op), `void cleanup()` (make current + `Rounded::cleanup`), `element_event_t onScroll(int,int,fpx_t)` (`{}`).

Protected helpers for subclasses: `bool beginRender()` (clear/viewport/blend + corner underlay; returns true when premultiplied alpha is active), `void drawTextVerts(const std::vector<float> &, const Ui::Color &, FontRenderer::font_rec_t &) const`, `void beginSvgDraw()`, `static void endSvgDraw()`. Protected members: `m_resManager`, `m_makeCurrent`, `m_width`/`m_height`, `m_fontRenderer`, `m_svgRenderer`, `m_rounded`.

### PopupRenderer

`ui/render/popup/popuprenderer.h` - `class PopupRenderer final : public PopupRendererBase`. Renderer for popup menus and submenus. Builds its element list once from a `menu_t` via `UiLayout::buildPopup`.

- `PopupRenderer(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager &, const Ui::Res::Type::menu_t & menu)`
- `static Ui::Res::Type::font_t boldVariant(Ui::Res::Type::font_t)` - bold-weight copy of a font (marks the active row in radio-group popups).
- `using SubmenuHoverFn = std::function<void(const bound_t &, const menu_t &, bool isFirst, bool isLast)>`; `void setSubmenuHoverCallback(SubmenuHoverFn)` - fired when hovering an item that has children, so the coordinator can open the submenu (bounds are rounded to physical pixels via `toPhysRound`).
- `void setSubmenuParentId(id_t)` - mark the row whose submenu is open (keeps it visually hovered; clears the previous parent).
- `const UiElement * findElementById(id_t) const`, `id_t hoveredItemId() const`, `void setHoveredItem(id_t)` - state query/restore used by WindowManager across resource reloads.
- `bool isFirstElement(id_t) const`, `bool isLastElement(id_t) const`.
- `void setContainerBorder(const Ui::Res::Type::border_t &)` - the popup container corner radii.
- `void updateCorner(id_t cornerIndex, const std::vector<uint8_t> & pixels, fpx_t radius)` - update a single captured corner.
- `bool render()` - `beginRender()` then draw container, item backgrounds/hover pills, theme-preview split swatches, item labels, per-item SVG icons (honoring `iconPlace`), preview letters, and right-aligned shortcut text.
- `void cleanup()` - clear elements + base cleanup.
- `Ui::IEventApp` overrides: `bool onMouseMove(int,int)` (hover + submenu-open detection), `bool onMouseLeave()`, `element_event_t onMousePress(int,int,MouseButton,int)`, `element_event_t onMouseRelease(int,int,MouseButton)`. Presses/releases only fire for enabled leaf `MenuItem`s (submenu parents open on hover).

### DialogRenderer

`ui/render/popup/dialogrenderer.h` - `class DialogRenderer final : public PopupRendererBase`. Renderer for modal dialog windows: centered dialog with title bar (optional icon + close X), a scrollable word-wrapped content area, an optional link line, and a centered button row.

Also defined here: `struct alignas(64) dialog_button_t final` - `{ bound_t bound; std::string label; UiElementState state; bool primary; Ui::Res::Type::DialogAction action; }` with defaulted `operator==`.

- `DialogRenderer(Ui::task_fn_t makeCurrent, const Ui::Res::ResManager &, const Ui::Res::Type::dialog_t & dialog)` - caches the dialog + close-icon SVG keys and builds the button row from the dialog-type config.
- `using DialogCloseFn = std::function<void(Ui::Res::Type::DialogAction)>`; `void setCloseCallback(DialogCloseFn)` - invoked with the chosen action when the dialog resolves.
- `void setRenderRequest(Ui::task_fn_t)` - callback to request another frame (used while animating scroll / keyboard press).
- `void confirmPrimary()` - fire the primary button's action.
- `void dismiss()` - close with `DialogAction::Ok`.
- `void onKeyPress(const std::string & key)` - keyboard nav: `Escape` animates the close button then dismisses; `Return` animates the focused (or primary) button then confirms; `Left`/`Right` move keyboard focus.
- `void resize(fpx_t,fpx_t)` - re-layouts buttons + close button.
- `bool render()` - advance smooth-scroll and key-animation, then draw the dialog (background, buttons, close X, icons, title, wrapped/scrolled content with scissor-clipped scrollbar, link, button labels).
- `void cleanup()` - clear buttons + base cleanup.
- `Ui::IEventApp` overrides: `bool onMouseMove(int,int)` (button/close hover + scrollbar thumb drag), `bool onMouseLeave()`, `element_event_t onScroll(int,int,fpx_t deltaY)` (scrolls content; honors `input().scrollNatural`/`scrollSpeed`), `element_event_t onMousePress(int,int,MouseButton,int)`, `element_event_t onMouseRelease(int,int,MouseButton)`.

Content scrolling is smoothed (exponential lerp toward a target); the scrollbar has a draggable thumb and track paging. Keyboard-triggered actions run a two-phase hover -> active -> fire animation timed off `input().keyAnimationDelay`.

---

## GL backend (Ui::Gl)

### GlRender

`ui/gl/glrender.h` - `class GlRender final : public Ui::IRender, private Common::NonCopyable`. The bottom concrete `Ui::IRender` implementation. Wraps `Rounded` (SDF rects), `SvgRenderer` (images), a `FontRenderer` (text), and a small flat-color shader (the loading-bar arrow triangle). Domain-blind: callers resolve all theme/layout values to plain args.

Batching: rects and images each need a `begin()`/`end()` GL setup, so the renderer tracks a `Mode` (`None`/`Rect`/`Image`) and switches lazily as primitive kinds change. Text is buffered and flushed font-sorted in `endFrame()` so it always composites above non-text.

- `GlRender(Ui::task_fn_t makeCurrent, const std::string & fontDir)` - `makeCurrent` makes the owning window's GL context current for teardown. Destructor makes the context current before deleting GL objects.
- `Ui::Gl::FontRenderer * fontRenderer()` - concrete accessor for popup/dialog renderers that share this font renderer (not part of `IRender`).

`Ui::IRender` overrides:

- `void beginFrame(fpx_t width, fpx_t height)` - cache size + scale, reset GL state, enable alpha blend, clear the text batch.
- `void fillRect(const bound_t &, const border_t & radii, const color_pair_t &, const shadow_t &)` - rounded rect via `Rounded` (optional layered soft shadow first).
- `void drawText(font_handle_t, std::string_view, const bound_t & pos, const Ui::Color &, AlignH, AlignV, fpx_t minPadH)` - buffer a text item (drawn in `endFrame`); alignment is resolved at flush via `TextAlign::startX`/`baselineY`.
- `void drawImage(std::string_view src, const bound_t &, const border_t &, const Ui::Color & tint, fpx_t scale, const shadow_t &)` - draw an SVG (tinted/scaled/shadowed variants chosen from the args; only `.svg` sources are drawn today). A transparent tint (`a == 0`) means untinted.
- `void warmImage(std::string_view src, const bound_t &, const Ui::Color & tint, fpx_t scale)` - pre-rasterize the scaled variant into the texture cache so a later scaled draw (press effect) is a cache hit.
- `void drawTriangle(fpx_t x0,y0,x1,y1,x2,y2, const Ui::Color &)` - flat-colored triangle via the flat shader (closes any open batch first).
- `void endFrame()` - close the current batch and flush the font-sorted text.
- `fpx_t textWidth(font_handle_t, std::wstring_view)` - measure via the font renderer.
- `font_handle_t createFont(const Ui::Res::Type::font_t &)` - create/reuse a font.

### Rounded

`ui/gl/rounded.h` - `class Rounded final : private Common::NonCopyable`. SDF-based rounded-rectangle renderer with per-corner radii and captured-corner-pixel support. THE single sanctioned path for every rounded shape - extend this class rather than writing a parallel SDF/scissor/stencil path. One instance per GL context. The SDF matches CSS `border-radius`: straight edges are fully opaque, corner arcs use inward smoothstep AA.

Two output modes controlled by `bgColor.a`: opaque composite (`a > 0`, blends fg/bg in-shader to a fully opaque output) or premultiplied alpha (`a == 0`, outputs `vec4(color*alpha, alpha)` for compositing over a capture layer or transparent window).

- `void setAlpha(bool)` / `bool hasAlpha() const`
- `static fpx_t borderRadius(const border_t &, id_t idx)` - radius by corner index (0=TL,1=TR,2=BR,3=BL).
- `void setCornerPixels(std::array<std::vector<uint8_t>,4> pixels, const border_t & radii)` - upload captured parent pixels as corner textures (each buffer `radius*radius*4` bytes RGBA, top-left origin; zero-radius/empty corners skipped).
- `void updateCorner(id_t cornerIndex, const std::vector<uint8_t> & pixels, fpx_t radius)` - replace a single corner texture.
- `bool hasCornerPixels() const`
- `void drawCorners(fpx_t viewW, fpx_t viewH, fpx_t scale)` - draw captured corner pixels as an underlay (call before `begin`/`draw`/`end`).
- `void begin(fpx_t viewW, fpx_t viewH, fpx_t scale)` - bind shader/VAO, set projection + AA (`1/scale`).
- `void draw(const bound_t &, const border_t & radii, const color_pair_t &) const` - one rounded rect (`colors.fg` = fill, `colors.bg` = AA/composite background).
- `void drawSplit(const bound_t &, const border_t & radii, const Ui::Color & colorL, const Ui::Color & colorR, fpx_t splitAngleDeg, const Ui::Color & bgColor) const` - a rounded rect filled with two colors split along an angled line (used for theme-preview swatches). `0` = horizontal split (`colorR` bottom), `45` = "/" line, `90` = vertical (`colorR` right).
- `static void end()` - unbind.
- `void cleanup()` - delete all GL objects (shaders, VAO/VBO, corner textures).

### SvgRenderer

`ui/gl/svgrenderer.h` - `class SvgRenderer : private Common::NonCopyable`. Header-only SVG rendering via librsvg + Cairo, with OpenGL texture caching. Loads/caches SVG documents process-wide, renders to a size-keyed texture cache, and draws textured quads. Supports tinting, scaling, and layered soft shadows.

Nested types: `struct cache_key_t { std::string svgId; int width; int height; }` (with `std::hash` specialization) and `struct svg_texture_t { GLuint textureId; int width; int height; bool valid; }`.

Frame + config:

- `static void setUploadPremultiplied(bool)` - upload textures premultiplied (for composited/ARGB popups); thread-local.
- `void begin(fpx_t viewWidth, fpx_t viewHeight, fpx_t scale)` / `static void end()` - bind/unbind the SVG shader + VAO for a batch.

Loading (static, cache the document, return a document id / key or empty on failure):

- `static std::string loadFromFile(const std::string & path)`
- `static std::string ensureLoaded(const std::string & path)` - load only if not already cached (does not bump the load counter).
- `static std::string loadFilledFromFile(const std::string & path)` - load with `fill="none"` replaced by `fill="#000000"` so outline-only icons can be tinted as solid shapes; id is prefixed `"filled:"`.
- `static std::string loadFromString(const std::string & svgData, const std::string & id)`

Textures + drawing:

- `svg_texture_t texture(const std::string & svgId, fpx_t width, fpx_t height = 0)` - get/render the texture at a size (physical px derived via scale; `height == 0` preserves aspect; auto-crops transparent padding when meaningful).
- `svg_texture_t shadowTexture(const std::string & svgId, fpx_t width, fpx_t height = 0)` - alpha-only shape mask for shadow rendering.
- `void draw(const std::string & svgId, const bound_t &)` - untinted.
- `void drawTinted(const std::string & svgId, const bound_t &, const Ui::Color &)`
- `void drawScaled(const std::string & svgId, const bound_t &, float scale = 0.9F)` and `void drawTintedScaled(..., float scale = 0.9F)` - centered, drawn at the raster's exact physical size to avoid resampling blur.
- `void drawWithShadow(...)` / `void drawTintedWithShadow(...)` - layered soft shadow (offset/blur/color/opacity) then the icon.

Size queries (static): `static bool intrinsicSizeIfCached(const std::string &, float & outW, float & outH)`, `static bool contentSizeIfCached(...)` (tight content size, padding excluded), `static std::pair<float,float> intrinsicSize(const std::string &)`, `static bool isLoaded(const std::string &)`.

Cache management: `void removeTexture(const std::string &, int w, int h)`, `void removeAllTextures(const std::string &)`, `void unload(const std::string &)`, `void clearCache()`, `static void clearGlobalCache()`, `void cacheStats(size_t & docs, size_t & texs, size_t & gpuMem) const`, `void printCacheStats() const`. Plus test-only instrumentation helpers (`intrinsicSizeCalled()`, `resetInstrumentationCounters()`).

### FontRenderer

`ui/gl/fontrenderer.h` - `class FontRenderer final : private Common::NonCopyable`. FreeType-based font loading with an OpenGL texture-atlas glyph cache and a text shader. Handles font resolution (res font dir scan + system fallbacks + per-codepoint fontconfig fallback + a synthesized "tofu" glyph), atlas packing, and text vertex generation with sRGB-correct blending. `constexpr bool FONT_DEBUG` / `LOG_TEXT_LAYOUT` gate diagnostics.

Nested types: `struct glyph_bitmap_t` (per-glyph size/offset/advance + atlas UVs + grayscale buffer) and `struct font_rec_t` (cache key `font_t`, resolved file path, `font_metrics_t`, FreeType faces, glyph map, 1024x1024 atlas + GL texture, and the text shader program/VAO/VBO/uniforms).

- `FontRenderer(Ui::task_fn_t makeCurrent, std::string fontDir)` - `makeCurrent` is used before GL teardown. Ref-counted so the last live instance calls `FcFini()`.
- `static std::atomic<int> & fcRefCount()`, `static FT_Library & ftLibrary()` - shared process-lifetime handles.
- `Ui::font_handle_t createFont(const Ui::Res::Type::font_t & font, font_metrics_t * fm = nullptr)` - create or reuse (cached by family+scaled-size+weight); pre-renders ASCII 32..126 into the atlas; optionally fills `fm`.
- `void deleteFont(Ui::font_handle_t)`
- `fpx_t textWidth(Ui::font_handle_t, std::string_view)` and `fpx_t textWidth(Ui::font_handle_t, std::wstring_view)` - CSS-pixel advance sum (lazily loads missing glyphs).
- `font_rec_t * font(Ui::font_handle_t)` - the record for rendering (or `nullptr`).
- `static int ftToPixels(int64_t ft26dot6)`
- `static FT_Face resolveFaceForCodepoint(font_rec_t &, uint32_t cp)`, `static void ensureGlyph(font_rec_t &, wchar_t)`, `static void commitGlyph(font_rec_t &, wchar_t, glyph_bitmap_t)`, `static void insertTofuGlyph(font_rec_t &, wchar_t)` - on-demand glyph loading chain.
- Text vertex builders (physical px, startX snapped to whole pixels for sharp `GL_LINEAR` sampling): `static void appendTextVerts(std::vector<float> &, font_rec_t &, std::wstring_view / std::string_view, float startX, float baseline)`, `static std::vector<float> buildTextVerts(...)`.
- `static float measureTextWidth(font_rec_t &, std::wstring_view / std::string_view)` - physical-px advance sum.
- `static std::vector<std::wstring> wrapText(font_rec_t &, std::wstring_view / std::string_view, float maxWidth)` - word-wrap (splits on `\n`, then spaces, then chars).

### font_metrics_t / font_style_t

`ui/gl/fonttypes.h` - `struct alignas(32) font_metrics_t final` (CSS px): `int height`, `ascent`, `descent`, `x_height`, `bool draw_spaces`. Baseline helpers (return physical-px Y, floored so glyphs stay pixel-aligned): `fpx_t baseline(fpx_t cssY, fpx_t cssH, fpx_t scale) const` (line-box centered, reserves descent) and `fpx_t baselineCap(fpx_t cssY, fpx_t cssH, fpx_t scale) const` (cap-height centered, matching CSS `text-box-edge: cap alphabetic`). Also `enum class font_style_t : unsigned char { normal, italic, bold }`.

### TextAlign

`ui/gl/textalign.h` - namespace `Ui::Gl::TextAlign`, free inline functions returning the physical-pixel start X for a text run inside a CSS box:

- `fpx_t startXLeft(const bound_t & box, fpx_t paddingHCss, fpx_t scale)`
- `fpx_t startXRight(const bound_t & box, fpx_t textWidthCss, fpx_t paddingHCss, fpx_t scale)`
- `fpx_t startXCenter(const bound_t & box, fpx_t textWidthCss, fpx_t scale)`
- `fpx_t startXCenterClamped(const bound_t & box, fpx_t textWidthCss, fpx_t minPaddingHCss, fpx_t scale)` - centered but never before `box.x + minPadding` (long labels left-align instead of overflowing).

### Ui::Gl::Util

`ui/gl/glutil.h` - namespace `Ui::Gl::Util`, shared GL helpers (all inline). `constexpr int POS_UV_FLOATS = 4`.

- `bool initGlLoader()` - load GL entry points once a context is current (GLEW off macOS; tolerates the benign `NO_GLX_DISPLAY` under EGL).
- `bool hasModernGl()` - true when the current context is GL >= 3.x (required by the core-profile pipeline).
- `bool isSoftwareRenderer()` - true when `GL_RENDERER` names a software rasterizer (llvmpipe, SwiftShader, GDI Generic, etc.).
- `void * bufferOffset(size_t bytes)` - the sanctioned int->ptr helper for vertex-attrib offsets.
- `GLuint compileShader(GLenum type, const char * source, const char * label)`, `GLuint linkProgram(GLuint vertex, GLuint fragment, const char * label)` - compile/link (0 on failure).
- `std::array<fpx_t,16> orthoProjection(fpx_t cssWidth, fpx_t cssHeight)` - top-left-origin, Y-down ortho.
- `void createPosUvVao(GLuint & vao, GLuint & vbo)` - VAO/VBO with vec2 pos (loc 0) + vec2 uv (loc 1).
- `std::array<float,24> quadVertices(fpx_t x, fpx_t y, fpx_t w, fpx_t h)` and `quadVerticesFlipY(...)` - 6-vertex textured quad.
- `template <typename Fn> void forEachShadowLayer(fpx_t offsetX, fpx_t offsetY, fpx_t blurRadius, fpx_t opacity, Fn fn)` - iterate 5 shadow layers outer to inner.
- `template <typename T, std::size_t N> void drawTriangles(const std::array<T,N> &, GLsizei vertexCount)` and `void drawTriangles(const std::vector<float> &)` - upload + draw.
- Safe deleters: `deleteProgram`, `deleteBuffer`, `deleteVertexArray`, `deleteTexture` (each takes `GLuint &`, zeroes it).

### GL header shim

`ui/gl/localglew.h` - include this instead of `<GL/glew.h>` / `<GL/gl.h>`. Pulls `<OpenGL/gl3.h>` on macOS (no loader needed) and `<GL/glew.h>` elsewhere.

---

## See also

- [interfaces.md](interfaces.md) - `Ui::IRender`, `Ui::IRenderer`, `Ui::IEventApp` contracts these classes implement.
- [vocabulary.md](vocabulary.md) - `intent_t`, `IntentKind`, `result_t`, `bound_t`, `border_t`, `color_pair_t`, `font_t`, `Ui::Color`, `g_config` and the CSS<->physical math.
- [windowing.md](windowing.md) - `Ui::Window::WindowManager` and the popup/dialog windows that own these renderers.
- [README.md](README.md) - documentation index and architecture overview.
