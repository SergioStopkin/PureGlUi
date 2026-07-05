# Project Guidelines

## Formatting
- Do not worry about code formatting - clang-format handles it automatically

## Search Scope
- Never search outside the project working directory unless the user explicitly asks. No `find /home/...`, no scanning other repos, no `~`. Stay inside the current project root.

## No User Data in Code
- Never mention real filenames, vendor / customer names, or any concrete test-file identifiers in source comments, logs, doc files, plan markdown, or commit messages. Describe the *condition* a file represents (e.g. "a file whose type has no registered handler", "a large resource set") instead of the literal name. Real paths are fine in chat and runtime diagnostics, never in artifacts that get committed.

## Understanding the Project

Always read these files first to understand the project structure and build system:
- `.cicd-config` - CI/CD configuration and project naming (`TARGET_SRC` = `pureglui`, `TARGET_TEST` = `unit-tests component-tests`)
- `build.sh` - Build script: `./build.sh <dev|rel|test>` (always use `rel` for builds; `test` enables `BUILD_TESTING`)
- `run.sh` - Run script: `./run.sh <dev|rel|test> [args...]`
- `CMakeLists.txt` - CMake configuration and dependencies (no OpenCASCADE; gtest is a test-only dep)

### Codebase Reading (required on session start)

The code lives in two include roots (see "Architecture: two roles" below): `include/common/` (depends on nothing) and `include/ui/` (the domain-blind UI framework + its runnable core `Ui::Shell`; depends only on `common`). The runnable app is a thin demo (`include/sig.h` + `src/`) that composes `Ui::Shell`. At the start of every session, read these header sets using parallel Task (Explore) agents, one per section:
1. **common/ + ui/ vocabulary** (`include/common/`, `include/ui/`): `common/bit.h`, `common/json.h`, `common/sanitize.h`, `common/unicode.h`, `common/fs.h`, `common/system.h`, `common/backgroundworker.h`; `ui/type.h`, `ui/color.h`, `ui/config.h`, `ui/const.h`, `ui/convert.h`, `ui/codepoint.h`, `ui/registry.h`, `ui/elementid.h`, `ui/tabbar.h`, `ui/action/registry.h`, `ui/action/actionmap.h`, `ui/intent.h`, `ui/intentkind.h`, `ui/result.h`, `ui/tab.h`, `ui/render/shadow.h`
2. **ui/ interfaces + render layer + GL backend** (`include/ui/`): `interface/irender.h`, `interface/ieventapp.h`, `interface/irenderer.h`, `render/uilayout.h`, `render/uirenderer.h`, `render/context.h`, `render/dockcolumn.h`, `render/uielement.h`, `render/uielementstate.h`, `render/clickresult.h`, `render/popup/popuprendererbase.h`, `render/popup/popuprenderer.h`, `render/popup/dialogrenderer.h`, `gl/localglew.h`, `gl/glutil.h`, `gl/glrender.h`, `gl/rounded.h`, `gl/svgrenderer.h`, `gl/fontrenderer.h`, `gl/fonttypes.h`, `gl/textalign.h`
2b. **ui/ windowing (backend + coordinator) + pubsub** (`include/ui/`): `pubsub/subscribe.h`, `pubsub/subscribeid.h`, `interface/iwindow.h`, `interface/ieventos.h`, `interface/icontext.h`, `window/windowbase.h`, `window/nativewindow.h`, `window/nativewindowhandle.h`, `window/nativedisplayhandle.h`, `window/event.h`, `window/eventfactory.h`, `window/clickcounter.h`, `window/eglcontext.h`, `window/platform/x11window.h`, `window/platform/x11event.h`, `window/platform/x11include.h` (+ wayland/macos/win32 window+event peers); the coordinator layer `window/windowmanager.h`, `window/renderqueue.h`, `window/compositetexture.h`, `window/contentsurface.h`, `window/contenthit.h`, `window/popup/popupwindow.h`, `window/popup/dialogwindow.h`
3. **ui/res framework** (`include/ui/res/`): `resmanager.h`, `respath.h`, `util.h`, `localemanager.h`, `store/*.h` (the 7 sub-stores: icon/dialog/shortcut/layout/theme/menu/dock), `type/*.h` (all UI value-type `*_t` structs + pure enums), `dock/*.h`
4. **ui/ runnable shell + actions + io** (`include/ui/`): `shell.h`, `action/exitapp.h`, `action/reload.h`, `action/switchtheme.h`, `action/switchthememode.h`, `action/openfile.h`, `io/filedialog.h`, `io/filefilter.h`; and the demo app `include/sig.h`, `src/main.cpp`, `src/sig.cpp`

## Architecture: two roles (common / ui) + a thin demo

The code is split across two include roots with a one-way dependency law (`ui -> common`):

- **`include/common/`** (`Common::`) - cross-cutting primitives that belong to no particular layer (`Bit`, `Sanitize`, `loadJson`, `Unicode`, `System`, `fs`). Depends on nothing.
- **`include/ui/`** (`Ui::`) - the **domain-blind UI framework**, and its **runnable core** `Ui::Shell`. Decides *what* to draw and *where*, and *what an input means* (as an intent). Knows menus, tabs, dialogs, themes, layout, shortcuts; treats `actionKey` as an opaque string and a tab as `{label,isActive,progress}`. Never knows what "OpenFile"/"a 3D model"/"a workspace" *is*. Depends on `common/` only. The GL backend (`ui/gl/`) is the bottom impl of `IRender`; the chrome (`ui/res/` + the render layer `ui/render/` - `UiLayout`/`UiRenderer`/`Context`/`DockColumn`, namespace `Ui::Render`) sits on top. It is host-free and has no host-domain knowledge.

`Ui::Shell` (`ui/shell.h`) is the runnable framework core: it owns `Ui::Res::ResManager` + `Ui::Window::WindowManager` + `Ui::Render::Context` + `Ui::Action::Registry`, and provides the init spine, the run loop, the default shell actions, the event->intent dispatch, and all interactive-chrome orchestration (popup/menu/dialog lifecycle, temp status). It runs standalone: `Ui::Shell shell; shell.initialize(); shell.run();`.

The **demo app** is a thin `src/main.cpp` (+ `src/sig.cpp`, `include/sig.h`) that constructs a `Ui::Shell`, wires `sig` to `shell.requestStop()`, and runs. A real **host** would build on the framework the same way and add its domain on top via the public seams - it is not a separate include root here.

Host seams (all `std::function`, no host-services interface):
- Content surfaces: `WindowManager::addContentSurface`/`removeContentSurface`/`setActiveContentSurface`/`setContentSurfaceReady` register host-owned child render surfaces (e.g. a 3D viewport). The fw drives `[main window] + content surfaces` blind through `IWindow` + `IRenderer` + `IEventApp` (no bespoke content interface). `IRenderer` carries a default `readPixels` (only a content-surface renderer overrides it) so the fw can composite any surface blind.
- Shell hooks: `setFrameTasks`/`setOnTick` (per-loop host work), `setOnTabActivated`/`setOnTabClosed`/`setOnKeyPress` (domain reactions), `setFileHandler(extension, fn)` (per-type file loaders for `OpenFile`).
- WindowManager hooks: `setOnDialogClose`/`setOnElementHover`/`setDoubleClickConfig`/`setDialogContentResolver`.
- `Ui::Res::ResManager` is the domain-blind resource facade composing 7 sub-stores (`ui/res/store/*`); it exposes persisted state via getters + an `onPersistChange` hook a host can fill.

Action subsystem (`ui/action/`, namespace `Ui::Action`): `Ui::Action::Registry` (`registry.h`) is the opaque `actionKey -> Ui::action_fn_t` dispatch table (`on`/`dispatch`/`has`). The built-in actions are one-per-header host-templated free functions (`exitapp.h`/`reload.h`/`switchtheme.h`/`switchthememode.h`/`openfile.h`); `actionmap.h::registerActions(shell)` binds them into the registry. A host registers its domain actions the same way via `shell.actions().on(key, fn)`. The `"action"` key comes from res JSON (menus/shortcuts); a menu item whose key has no registered handler is greyed out by the disable pass (`disableUnhandledMenuItems`). There is no runtime name->function reflection in C++, so the key->function binding is an explicit list (see `.work/action-binding-reflection.md`).

The render-chrome layer lives in `ui/render/` (namespace `Ui::Render`): `UiLayout`/`UiRenderer`/`Context`/`DockColumn` + the value types (`UiElementType`/`UiElementState`/`click_result_t`), plus the popup-chrome renderers `ui/render/popup/` (`Ui::Render::Popup`: `PopupRendererBase`/`PopupRenderer`/`DialogRenderer`). The renderer/event CONTRACTS `Ui::IEventApp` + `Ui::IRenderer` live in `ui/interface/` (a host content renderer implements `Ui::IRenderer`). `UiRenderer` is window-free: it takes a `makeCurrent` callback + caches size.

The platform windowing backend lives in `ui/window/` (namespace `Ui::Window`): `WindowBase` + the per-OS windows/event handlers (`platform/`), `EglContext` (the `IContext` GL-context impl), `Event`/`EventFactory`/`ClickCounter`, native-handle aliases, and the `IWindow`/`IEventOS`/`IContext` seams. The pub/sub dispatcher is in `ui/pubsub/` (`Ui::PubSub`).

The windowing coordinator lives in `ui/window/` (namespace `Ui::Window`): `WindowManager` (main window + popups/submenus/dialogs + event loop + render queue + content-surface registry + Wayland compositing), the popup windows `window/popup/` (`Ui::Window::Popup`: `PopupWindow`/`DialogWindow`), `RenderQueue`, and the content/composite helper types (`content_surface_t`/`ContentHit`/`CompositeTexture`). It consumes the `ui/window/` primitives + `Ui::Render` chrome and reads compositing/scale from `Ui::g_config`.

## Code Style

- Simplicity first: code should be as small and simple as possible. Prefer the simplest correct solution over clever or elaborate approaches.
- All files must end with a newline character
- Never use any unicode symbols in code or comments
- Use ASCII only
- Never use em-dash or en-dash in code or comments - use a single hyphen (-) instead
- Always add `final` to class/struct if not intended as a base class
- One type (struct/enum/class) per header file

### Struct Rules

- All structs use `_t` suffix naming: `shortcut_t`, `bound_t`, `click_result_t`
- All structs use `alignas()` specifier, e.g.: `struct alignas(128) shortcut_t final`
- Always reuse existing code; keep the codebase as small as possible
- Avoid code duplication: extract shared logic into reusable functions, factories, or base classes
- Never create a new entity (field, variable, type) when an existing one can be reused or derived from
- When asked "do we still need X?" - check whether the same logic already exists elsewhere (duplication), not just whether X is still referenced. If the logic is duplicated, remove the old version.
- Never create temporary files - use in-memory I/O (pipes, streams, buffers) instead
- Never use forward declarations; always include the required header directly
- Never use partial namespace blocks; always use fully-qualified namespace paths (e.g. `Ui::Render::Popup::PopupRenderer`)
- Never use `static_cast` between our own types; fix the type at the source instead. Casts are only acceptable at external API boundaries (GL, FreeType, Wayland, Win32, std lib size_t). If a cast is needed between our types, the types are wrong.
- Always use `static_cast`, never `reinterpret_cast`. Where an object-pointer pun is genuinely required (C API byte buffers, COM out-params, Win32 header puns), spell it as the two-step `static_cast` through `void*` wrapped in a NAMED helper (`Common::asBytes`, `Common::viewCString`, `FileDialog::comOut`, `Win32Window::asBitmapInfo`) - never inline. `dynamic_cast` is allowed when a runtime type check is truly needed, but prefer restructuring to `static_cast`-free designs (typed ownership). Sole sanctioned `reinterpret_cast` exceptions - conversions `static_cast` cannot express at all: integer<->pointer (GL vertex offsets, Win32 `LONG_PTR`/`LPARAM`) and function-pointer results of `eglGetProcAddress`-style loaders; each stays in a named helper or carries a NOLINT with a reason.
- Never add new `#pragma clang diagnostic` (push/ignored/pop) blocks. If a warning fires, fix the root cause - pick a non-deprecated API, adjust the code, or suppress the warning project-wide in CMake if it's truly a systemic false positive. Silencing at the call site hides real signals. Two existing exceptions are sanctioned (do NOT remove or copy the pattern elsewhere): `std::codecvt_utf8` wide-stream deprecation in `windowmanager.h`, and `-[NSOpenGLContext setView:]` in `ui/window/platform/macoswindow.h` (entire NSOpenGL stack is deprecated in favor of Metal).
- Use `fpx_t` for any `float` field, not just pixel values - the important thing is the underlying `float` type. Use `bound_t` for any x+y+w+h group of `fpx_t` fields instead of separate variables.
- Never use `float`/`double`/`fpx_t` as loop counters - floating-point iteration accumulates rounding errors. Use `int` loops and let implicit promotion handle comparisons with `fpx_t` bounds. Use `while` when the loop variable type differs from the bound type.
- Use full descriptive names for function parameters, not abbreviations: `window` not `win`, `texture` not `ct` or `tex`, `renderer` not `rdr`. Short names are acceptable only when widely known: `str`, `fn`, `idx`, `it`
- Keep consistent parameter ordering across related functions: if one function takes `(window, texture)`, all similar functions must use the same order
- `const` parameters go first in function signatures, non-const (output/mutable) parameters go last
- TODO comments must include author: `TODO(sergio):`, never plain `TODO:`
- Prefer `emplace_back` over `push_back` when constructing elements in-place
- Prefer pure C++ over C APIs: use `std::string_view`, `std::copy_n`, `std::array`, `std::ofstream`/`std::ifstream` instead of `strlen`, `strcmp`, `strstr`, `strncpy`, `memcpy`, `fopen`/`fwrite`/`fclose`. C string functions are acceptable only at C API boundaries (Wayland, ObjC, Win32) where the API returns `const char *`
- All statements, loops, switch-cases, and try-catches must use braces:
  ```cpp
  // Good
  if (condition) {
      doSomething();
  }

  // Bad
  if (condition)
      doSomething();
  ```

## Resource Management

- All UI parameters live in `./res/*` JSON files; `ResManager` must parse all of them
- All code must read UI values exclusively from `ResManager` - never create local copies or duplicates
- Never hardcode resource names (theme names, locale keys, etc.) in JSON files - all content must be data-driven (discovered from filesystem, generated from data, etc.)
- Theme display names come from the `"name"` field inside each theme JSON (e.g. `"name": "Default"`), NOT from the filename stem. Filenames `<key>-dark.json` / `<key>-light.json` are only pairing keys; UI must show the JSON `"name"` resolved via `LocaleManager`.
- For every per-file resource directory (menus, themes, future presets), the routing key / identity is the JSON `"name"` field. Filenames are cosmetic - they may carry a leading `NN.` ordering prefix for filesystem-listing convenience but loaders must NOT extract identity from the filename. Display labels come from `LocaleManager.get(name)` so locale files can capitalise/translate the canonical key.
- Every string a loader ingests from res JSON passes the type-appropriate `Common::Sanitize` at parse: `filePath` for path fields, `url` for real URLs, `string` for everything else (labels, keys, titles, content). res/ is a trust boundary (hosts and users edit these files); sanitizing at ingest protects every downstream sink (logs, rendering, routing) in one place. Audit rule: any `.value("` in a loader must be wrapped (bool/number fields excepted).

## Graphics Stack

- We use EGL, not GLX
- We do not use GLFW
- OpenGL context is created via EGL directly
- All rounded shapes (rounded rectangles, ellipses, pills, capsules - any shape with corner radii) MUST go through the `Rounded` class. The only exception is a host content surface, which renders its own content. If `Rounded` does not yet support a needed mode (split fills, gradients, etc.), extend `Rounded` rather than writing a parallel SDF/scissor/stencil path elsewhere.

## Window Architecture

- Main window: creates an X11/Wayland window + EGL context for UI rendering
- Content surfaces: a host embeds child render surfaces (X11 child window or Wayland subsurface); the host owns and manages each surface's own GL context. The framework drives them blind through `IWindow` + `IRenderer`
- Runtime platform detection: check `WAYLAND_DISPLAY` env var to choose Wayland vs X11

## UI/CSS Architecture

- `res/css/layout.json` - Layout properties (dimensions, positioning, structure)
- `res/submenu/theme/<key>-dark.json` / `<key>-light.json` - Theme colors and fonts (backgrounds, text colors, `:root` CSS vars like `--cl-main`/`--cl-info`/`--cl-warn`)
- Layout defines structure (height, margin, position), theme defines appearance (colors, fonts)
- JSON keeps CSS property names (`border-radius`); the matching C++ field is element-prefixed (`gripRadius`, not `borderRadius`)

## File Organization

- Prefer header-only files where possible (`*.h` in `include/`, `*.cpp` only in `src/`)
- Platform-specific code goes in `platform/` subdirectories
- Use CRTP for compile-time polymorphism where appropriate

## Header Files

Two include roots (see "Architecture: two roles"). Paths below are relative to each root.

### common/ (`Common::`)

- `bit.h` - `Bit`: bitflag helpers (`Or`/`And`) used for `Changed` flags etc.
- `json.h` - `loadJson(file, json&) -> bool`: domain-blind JSON file parse, shared by the fw loaders.
- `sanitize.h` - `Sanitize`: string/URL/path validators (strip control chars, length caps, log on modify).
- `unicode.h` - `Unicode`: UTF-8 <-> wide conversions for the text pipeline.
- `fs.h` - filesystem helpers (`dirExists`).
- `system.h` - `System`: general host info (CPU cores + base clock, RAM, process name). OS primitive; depends only on the platform + `Common::Unicode`.
- `backgroundworker.h` - `BackgroundWorker` + process-scoped `backgroundWorker()`: a CPU-budgeted async task pool (max(1, cores-2) threads) for moving heavy work off the UI thread. Depends only on `Common::System`.

### ui/ - framework vocabulary (`Ui::`)

- `type.h` - `Ui::` aliases: `id_t`, `fpx_t`, `key_t`, `action_fn_t` (`std::function<void(const std::string&)>`), `INVALID_ID`.
- `const.h` - `Ui::PI` (fpx_t, via `std::numbers`): shared degree<->radian constant.
- `codepoint.h` - `Ui::Codepoint` enum + `Ui::wstr()`: named Unicode UI-chrome glyphs (ellipsis etc.).
- `color.h` - `Ui::Color`: RGBA bytes, `fromHex`, `Lighter`/`Darker`, premultiplied alpha.
- `config.h` - `config_t` + canonical `g_config` (dpi/scale/compositing) and CSS<->physical math: `toPhys`/`toPhysFloor`/`toPhysRound`/`toCss`/`toCssFloor`/`roundToInt`.
- `convert.h` - `Ui::Convert`: CSS string conversion (`str2int`/`str2fpx`/`str2uint32`) + `parseCssNumber`/`parseCssInt`/`parseCssBorderRadius`.
- `registry.h` - `Registry<T>`: generic ordered id-keyed container (unordered_map + order vector).
- `elementid.h` - `Ui::ElementId` enum: element-id range starts (`MenuBase`/`ButtonBase`/`ItemBase`) the loader assigns from.
- `tabbar.h` - `Ui::TabBar`: tab view-model (`Registry<tab_t>` + scroll); a host projects its tabs into it.
- `action/registry.h` - `Ui::Action::Registry`: opaque `actionKey -> action_fn_t` dispatch table (`on`/`dispatch`/`has`).
- `intent.h` / `intentkind.h` / `result.h` - intents-out vocabulary: `intent_t` (tagged), `IntentKind`, `result_t {isDirty, intents}`.
- `type/tab.h` - `tab_t`: tab view-model `{id,label,isActive,isLoading,progress}`.
- `render/shadow.h` - `Ui::Render::shadow_t`: drop-shadow params (`{}` = none).

### ui/ - interfaces + render layer + GL backend

- `interface/irender.h` - `Ui::IRender`: immediate draw sink (fillRect/drawText/drawImage/drawTriangle + measurement + createFont). GL is one impl.
- `interface/ieventapp.h` - `Ui::IEventApp`: pointer-event sink (onMouseMove/Press/Release/Leave/Scroll, returns `Ui::Render::click_result_t`). Self-rooted. Base of `Ui::IRenderer` + `Ui::IWindow`/`Ui::Window::WindowManager`.
- `interface/irenderer.h` - `Ui::IRenderer`: renderer-object lifecycle (render/resize/apply(Changed)/refresh/cleanup/statusText + default `readPixels`). Distinct from `IRender` (the draw sink). Implemented by `Ui::Render::UiRenderer` + the `Ui::Render::Popup` renderers + any host content renderer.
- `render/uilayout.h` - `Ui::Render::UiLayout`: lays out the fixed UI regions into `UiElement`s, draws via `Ui::IRender`. `render/uirenderer.h` - `Ui::Render::UiRenderer`: walks the layout, emits IRender ops, owns the GL `GlRender`; window-free (makeCurrent callback + cached size). `render/context.h` - `Ui::Render::Context`: maps clicks/keys -> `Ui::intent_t`. `render/dockcolumn.h` - `Ui::Render::DockColumn`. `render/uielement.h`/`uielementstate.h`/`clickresult.h` - `Ui::Render` value types (`UiElementType`/`UiElementState`/`click_result_t`).
- `render/popup/*` - `Ui::Render::Popup`: `PopupRendererBase` (shared GL setup/text/SVG/corner mgmt + `IRenderer` boilerplate + software-rounded corner support (setCornerPixels/hasCornerPixels/setAlpha); owns a `FontRenderer` by value, makeCurrent injected - renderers never touch `IWindow`), `PopupRenderer` (menu/submenu popups), `DialogRenderer` (modal dialogs: title/content/buttons/scroll/keyboard nav). Domain-blind chrome.
- `gl/glrender.h` - `Ui::Gl::GlRender`: `IRender` impl over Rounded/Svg/Font + a flat shader; batches by mode, text font-sorted at endFrame.
- `gl/rounded.h` - `Rounded`: SDF rounded-rect renderer + corner pixel capture (the ONLY path for rounded shapes).
- `gl/svgrenderer.h` - `SvgRenderer`: SVG load/cache via librsvg/Cairo -> GL texture, premultiplied alpha.
- `gl/fontrenderer.h` / `fonttypes.h` / `textalign.h` - FreeType atlas + text shader; `font_handle_t`/`font_metrics_t`; alignment enum.
- `gl/glutil.h` - `Ui::Gl::Util`: shared GL helpers (shader compile/link, VAO, pos/uv layout).
- `gl/localglew.h` - GL header shim (GLEW on Linux/Windows, `<OpenGL/gl3.h>` on macOS).

### ui/ - windowing (backend + coordinator) + pubsub (`Ui::Window`, `Ui::PubSub`)

- `interface/iwindow.h` - `Ui::IWindow`: the surface seam (native handles + create/resize/show/makeCurrent/swap + `render`/`refresh`/`requestRender`/`setRenderRequest`), a `Ui::IEventApp`. `ieventos.h` - `IEventOS`: OS event polling + clipboard (self-rooted). `icontext.h` - `IContext`: GL-context abstraction (self-rooted).
- `window/windowbase.h` - `WindowBase<Derived>` CRTP (owns the `Ui::IRenderer`, render-request wiring, geometry). `nativewindow.h`/`nativewindowhandle.h`/`nativedisplayhandle.h` - the `NativeWindow` alias + handle types (compile-time platform dispatch). `platform/{x11,wayland,macos,win32}window.h` - per-OS `IWindow` impls; `platform/*event.h` - per-OS `IEventOS` impls; `platform/x11include.h` - X11 header shim. `eglcontext.h` - `EglContext`: EGL context (X11/Wayland), the `IContext` impl. `event.h` (`Event`/`EventType`/`MouseButton`/`KeyModifier`), `eventfactory.h`, `clickcounter.h`.
- `window/windowmanager.h` - `Ui::Window::WindowManager`: the windowing coordinator (main window + popups/submenus/dialogs + event polling/dispatch + render queue + content-surface registry + Wayland compositing). Drives `[main window] + content surfaces` blind through `IWindow`/`IRenderer`/`IEventApp`; reads scale/compositing from `Ui::g_config`. App-facing seams are `std::function` setters (`setOnDialogClose`/`setOnElementHover`/`setDoubleClickConfig`/`setDialogContentResolver`) + accessors (`subscribe()`/`renderQueue()`/`mainWindow()`) + the popup/dialog/content-surface API.
- `window/renderqueue.h` - `Ui::Window::RenderQueue`: set of window ids needing redraw. `window/compositetexture.h` (`CompositeTexture` - offscreen->texture cache for Wayland compositing), `window/contentsurface.h` (`content_surface_t` - `{IWindow*, IRenderer*, CompositeTexture, isReady}`), `window/contenthit.h` (`ContentHit` - hit-test result with surface-local coords) - content-surface registry types.
- `window/popup/popupwindow.h` - `Ui::Window::Popup::PopupWindow : Ui::Window::NativeWindow`: borderless always-on-top popup (X11 ARGB visual / Wayland xdg_popup / per-corner shape), owns a `Ui::Render::Popup` renderer. `followsParent()` reports when it is an X11 child of the main window (the server moves/re-anchors it, so the shell must not manually track move/resize). `window/popup/dialogwindow.h` - `DialogWindow : PopupWindow`: modal dialog (centering, close callback). Domain-blind.
- `pubsub/subscribe.h` - `Ui::PubSub::Subscribe`: in-process pub/sub. `pubsub/subscribeid.h` - `SourceId`/`SubscriberId` ranges (window/content/popup/dock/event/action; element ids in `ui/elementid.h`).

### ui/res - resource framework (`Ui::Res::`)

- `resmanager.h` - `ResManager`: domain-blind resource facade composing the 7 sub-stores below + locale/resPath/tabBar/status/activeMenu + the persisted-setting registry (window geometry, last-open dir, open-files list, theme); exposes state + an `onPersistChange` hook for a host. Public accessors delegate to the stores. `findMenuItem(id)` / `findMenuItemByKey(key)` resolve a menu node; `disableUnhandledMenuItems(isHandled)` greys leaf action items with no handler.
- `respath.h` - `ResPath`: resolves bundled `res/` paths (icons, fonts, themes, layouts, locales), forward-slash output.
- `util.h` - `Util`: `strKey` etc. (shortcut-key normalization).
- `localemanager.h` - `Ui::Res::LocaleManager`: locale load + string lookup by JSON key.
- `store/iconstore.h` / `dialogstore.h` / `shortcutstore.h` / `layoutstore.h` / `themestore.h` / `menustore.h` / `dockstore.h` - `Ui::Res::Store::*`: the logical sub-loaders ResManager composes (icon defaults; dialog config; shortcuts; layout+popup; theme+name/mode+preview+scanThemeNames; menus/buttons/action-map/auto-submenu; session per-dock state). Loaders that diff return a `Changed` bit the facade ORs.
- `type/*.h` - `Ui::Res::Type` UI value types: `bound`, `border`, `region`, `font`, `colorpair`, `changed`, `popup`, `theme`, `themepreview`, `layout`, `menu`, `button`, `dialog`, `icondefault`, `iconplace`, `input` (UI scroll/key-anim half).
- `dock/*.h` - `Ui::Res::Dock`: `anchor`, `config`, `layout`, `state`, `theme`.

### ui/ - runnable shell + actions + io (`Ui::`, `Ui::Action`, `Ui::Io`)

- `shell.h` - `Ui::Shell`: the runnable framework core. Owns `ResManager` + `WindowManager` + `Context` + `Action::Registry`; provides `initialize()`/`run()`/`requestStop()`/`shutdown()`, the default-action binding (`Ui::Action::registerActions(*this)`), the event->intent dispatch, popup/menu/dialog lifecycle (`createMenuPopup`/`reloadChrome`/`reopenActivePopup`/`closePopupMenu`), the active-menu key save/restore across reload+move+resize, `disableUnhandledMenuItems`, `resolveDialogPlaceholders` (%VERSION%/%CPU%/%GPU%/%GL%/%RAM%), and the host hooks (`setFrameTasks`/`setOnTick`/`setOnTabActivated`/`setOnTabClosed`/`setOnKeyPress`/`setFileHandler`).
- `action/registry.h` - `Ui::Action::Registry` (see vocabulary). `action/actionmap.h` - `Ui::Action::registerActions(host)`: binds every built-in action into the host's registry (a plain iterate-once list, not a second map). `action/{exitapp,reload,switchtheme,switchthememode,openfile}.h` - one built-in action per header, each a host-templated free function in `Ui::Action`. `openfile.h` also carries `fileExtension` + the no-handler warning dialog.
- `io/filefilter.h` - `Ui::Io::file_filter_t`: one open-dialog file-type filter (`{name, spec}`; spec = `"*.step;*.iges"`). `io/filedialog.h` - `Ui::Io::FileDialog::openFiles(startDir, title, filters)`: native open dialog (Win32/macOS/zenity+kdialog). Domain-blind: filters come from res JSON; empty filters => any file.

### app / demo (`include/sig.h`, `src/`)

- `include/sig.h` - `g_app_init(std::function<void()>)` + signal handling for graceful shutdown (stores a stop callback; not tied to any concrete app type).
- `src/main.cpp` - constructs a `Ui::Shell`, wires `sig` to `shell.requestStop()`, runs `initialize()` + `run()`. `src/sig.cpp` - the `sig` implementation. A real host adds content surfaces + domain actions + file handlers + domain hooks on top of the same `Ui::Shell`.
