# ui/render + ui/gl - chrome renderer and GL backend

The render-chrome layer (`Ui::Render`, `Ui::Render::Popup`) is the domain-blind UI chrome: it lays out the fixed main-window regions, popups, and dialogs, and draws them through the abstract draw sink `Ui::IRender`. The GL backend (`Ui::Gl`) is the bottom concrete `IRender` implementation (`GlRender`) plus its building blocks (SDF rounded rects, SVG textures, FreeType text).

Layering, top to bottom:

- `Ui::Render::UiRenderer` and the `Ui::Render::Popup` renderers implement `Ui::IRenderer` (renderer-object lifecycle: render/resize/apply/cleanup - see [interfaces.md](interfaces.md)) and are self-rooted `Ui::IEventApp`s (pointer-event sinks).
- They emit primitives through `Ui::IRender` (fillRect/drawText/drawImage/drawTriangle/measure/createFont). `UiRenderer` holds the sink as an injected `std::unique_ptr<Ui::IRender>`, so it is decoupled from GL: `Ui::Gl::GlRender` in the app, a fake sink in headless tests.
- `Ui::Render::Context` never draws; it maps clicks/keys to `Ui::intent_t` (intents-out; see [vocabulary.md](vocabulary.md)).

House rule for rounded shapes: every rounded rectangle, pill, split-fill, or corner-rounded surface MUST go through `Ui::Gl::Rounded`. Do not write a parallel SDF/scissor/stencil path anywhere else - if `Rounded` lacks a mode you need, extend `Rounded`. The only exception is a host content surface, which renders its own content.

All chrome geometry is in CSS pixels; the GL backend scales to physical pixels via `Ui::g_config.scale`. Header paths below are relative to `include/`.

---

## Render chrome (Ui::Render)

### UiElementType

`ui/render/uielement.h` - `enum class UiElementType : unsigned char`. The kinds of element `UiLayout` produces: `MenuButton`, `ToolbarButton`, `MenuItem`, `Separator`, `Text`, `Image`, `Tab`, `TabClose`, `TabArrow`. Free helper `int toInt(UiElementType)`.

### UiElementState

`ui/render/uielementstate.h` - `enum class UiElementState : unsigned char { None = 0, Hovered, Active, Disabled }`. Interaction state of a laid-out element. `Disabled` elements are inert (skipped by hit-test and hover). Free helper `int toInt(UiElementState)`.

### UiElement

`ui/render/uilayout.h` - `struct alignas(32) UiElement final`. One laid-out element:

- `UiElementType type` (default `MenuButton`)
- `UiElementState state` (default `None`)
- `id_t id` (default `INVALID_ID`)
- `Ui::Res::Type::bound_t bound`

Content (text, icons, shortcuts) and appearance (colors, fonts) are NOT stored here - renderers resolve them from `ResManager` at render time by `type` + `id`.

### UiLayout

`ui/render/uilayout.h` - `class UiLayout final`. Lightweight layout engine: turns `layout_t`/`theme_t`/menu/button/tab data into a flat `std::vector<UiElement>` for direct rendering. Coordinates are CSS pixels. Reserved element ids (file-scope constants): `TAB_ARROW_LEFT = 9997`, `TAB_ARROW_RIGHT = 9998`, `STATUS_TEXT_ID = 9999`.

Methods:

- `void build(const layout_t &, const theme_t &, const std::vector<menu_t> &, const std::vector<button_t> &, const TabBar &, const LocaleManager &, fpx_t windowCssW, fpx_t windowCssH, Ui::IRender * render, const std::string & statusText = "")` - build the main-window layout: top-menu buttons (left-aligned label menus, right-aligned square icon/action menus), left toolbar buttons, status-bar text, and workspace tabs. Resolves and caches the font handles via `render->createFont`. `render` may be null (measurement falls back to `layout.fallbackCharWidth`).
- `static std::vector<UiElement> buildPopup(const menu_t & menu, const ResManager &)` - build popup dropdown items (`MenuItem` rows + `Separator`s) from menu data.
- `UiElement * hitTest(fpx_t cssX, fpx_t cssY)` - topmost non-disabled element at CSS coords, or `nullptr`. Iterates in reverse (last drawn wins).
- `UiElement * elementById(id_t)` - first element with matching id, or `nullptr`.
- `const UiElement * parentOf(const UiElement & child) const` - the element sharing the child's id but with a different type (e.g. a `Tab` for a `TabClose`), or `nullptr`.
- `const std::vector<UiElement> & elements() const` / `std::vector<UiElement> & elements()` - the flat draw list (draw in order).
- Cached font-handle getters: `menuFont()`, `itemFont()`, `itemFontBold()`, `popupFont()`, `statusBarFont()` (all `Ui::font_handle_t`).
- `int buttonImgSize() const` - toolbar-button image size (CSS px).
- `static std::string truncateFileName(const std::string & text, fpx_t maxW, Ui::IRender * render, Ui::font_handle_t font)` - fit a filename into `maxW` via a degradation ladder (full -> prefix+ellipsis+ext -> prefix+ellipsis -> ellipsis -> empty). Works on `std::wstring` internally so it slices by codepoint, not byte. Returns the input unchanged when measurement is unavailable (null render or `font == 0`).

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

`Ui::IEventApp` overrides: `bool onMouseMove(int,int)`, `bool onMousePress(int,int,int clickCount)`, `click_result_t onMouseRelease(int,int)`, `bool onMouseLeave()`, `bool onScroll(int,int,fpx_t)` (no-op, returns false). All translate physical input to CSS via `toCss`, update element `state`, and report whether a redraw is needed.

Internally, `generateDrawOps()` resolves colors/fonts/text/image-src per element from `ResManager` (`resolveColors`/`resolveFont`/`resolveText`/`resolveImageSrc`), and `flushOps()` translates the staged op buffers into `IRender` primitive calls in the order backgrounds -> progress -> images -> text.

### Context

`ui/render/context.h` - `class Context final : private Common::NonCopyable`. The intents-out facade. It interprets resolved main-UI input against `ResManager` data and returns intents; it never runs an action, opens a window, or mutates state.

- `explicit Context(const Ui::Res::ResManager &)`
- `Ui::result_t mapClick(const click_result_t & click, id_t openMenuId) const` - map a resolved click to intents. Examples: a `MenuButton` with an action key -> `EmitAction`; otherwise toggles `OpenPopup`/`ClosePopup` against `openMenuId`. A `MenuItem` leaf -> `ClosePopup` then `OpenDialog` (if it has a dialog) or `EmitAction`; a submenu parent yields nothing (opened on hover). `Tab` -> `SwitchTab`, `TabClose` -> `CloseTab`, `ToolbarButton` -> `EmitAction`, `Text` (status bar) -> `CopyText`.
- `Ui::result_t mapKey(const std::string & normalizedKey) const` - resolve a normalized keyboard shortcut against `shortcuts.json` to an `EmitAction` intent (empty result if unbound).

See [vocabulary.md](vocabulary.md) for `intent_t`, `IntentKind`, and `result_t`.

### DockColumn

`ui/render/dockcolumn.h` - `class DockColumn final : private Common::NonCopyable`. One collapsible dock column anchored to the left or right edge. Owns the live width + expanded state for one dock and reads/writes it through `ResManager` so session persistence is transparent. Compile-time `constexpr bool DOCK_DEBUG = false`.

- `DockColumn(id_t id, const Ui::Res::Dock::dock_config_t & cfg, const Ui::Res::ResManager &)` - seeds `memoryX` from `cfg.defaultWidth` on first use.
- Accessors: `id()`, `name()`, `anchor()` (`DockAnchor`), `order()`, `currentWidth()` (content + grip, transient width while dragging), `isDragging()`, `isHoveredGrip()`, `outer()`, `content()`, `grip()` (all bounds in CSS px).
- Clamping: `clampTransientWidth(fpx_t maxExpanded)`, `clampStateWidth(fpx_t maxExpanded)` (also clamps `memoryX`, persists if changed).
- Mouse routing (CSS coords, return whether visual state changed): `bool isOverGrip(fpx_t,fpx_t) const`, `bool onMouseMove(fpx_t,fpx_t)`, `bool onMouseDown(fpx_t,fpx_t,int clickCount)` (double-click on the grip toggles collapse/restore; single starts a drag), `void onMouseUp(fpx_t cssX)`, `void onMouseLeave()`. Capture decisions live one layer up (WindowManager).
- `void setLayout(fpx_t y, fpx_t h, fpx_t innerEdgeX)` - position the dock; `innerEdgeX` is the viewport-facing edge.
- `void render(UiRenderer & out) const` - emit content-background, grip-strip, and grip-glyph ops via the renderer's extra-ops hook. The grip strip and glyph use `appendBg`/`appendImage`; only the viewport-facing corners are rounded (`gripRadius`), and the glyph is the `--dock-grip-icon` SVG tinted with the grip color.

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

`Ui::IRenderer` boilerplate: `void resize(fpx_t,fpx_t)` (caches size), `void apply(Changed)` (no-op), `void cleanup()` (make current + `Rounded::cleanup`), `bool onScroll(int,int,fpx_t)` (false).

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
- `Ui::IEventApp` overrides: `bool onMouseMove(int,int)` (hover + submenu-open detection), `bool onMouseLeave()`, `bool onMousePress(int,int,int)`, `click_result_t onMouseRelease(int,int)`. Presses/releases only fire for enabled leaf `MenuItem`s (submenu parents open on hover).

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
- `Ui::IEventApp` overrides: `bool onMouseMove(int,int)` (button/close hover + scrollbar thumb drag), `bool onMouseLeave()`, `bool onScroll(int,int,fpx_t deltaY)` (scrolls content; honors `input().scrollNatural`/`scrollSpeed`), `bool onMousePress(int,int,int)`, `click_result_t onMouseRelease(int,int)`.

Content scrolling is smoothed (exponential lerp toward a target); the scrollbar has a draggable thumb and track paging. Keyboard-triggered actions run a two-phase hover -> active -> fire animation timed off `input().keyAnimationDelay`.

---

## GL backend (Ui::Gl)

### GlRender

`ui/gl/glrender.h` - `class GlRender final : public Ui::IRender, private Common::NonCopyable`. The bottom concrete `Ui::IRender` implementation. Wraps `Rounded` (SDF rects), `SvgRenderer` (images), a `FontRenderer` (text), and a small flat-color shader (the loading-bar arrow triangle). Domain-blind: callers resolve all theme/layout values to plain args.

Batching: rects and images each need a `begin()`/`end()` GL setup, so the renderer tracks a `Mode` (`None`/`Rect`/`Image`) and switches lazily as primitive kinds change. Text is buffered and flushed font-sorted in `endFrame()` so it always composites above non-text.

- `GlRender(Ui::task_fn_t makeCurrent, const std::string & fontDir)` - `makeCurrent` makes the owning window's GL context current for teardown. Destructor makes the context current before deleting GL objects.
- `Ui::Gl::FontRenderer * fontRenderer()` - concrete accessor for popup/dialog renderers that share this font renderer (not part of `IRender`).

`Ui::IRender` overrides:

- `void beginFrame(fpx_t width, fpx_t height)` - `glFinish`, cache size + scale, reset GL state, enable alpha blend, clear the text batch.
- `void fillRect(const bound_t &, const border_t & radii, const color_pair_t &, const shadow_t &)` - rounded rect via `Rounded` (optional layered soft shadow first).
- `void drawText(font_handle_t, std::string_view, const bound_t & pos, const Ui::Color &, bool centered, fpx_t minPadH)` - buffer a text item (drawn in `endFrame`).
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
