# PureGlUi API Documentation

PureGlUi is a domain-blind C++20 desktop UI framework built directly on EGL/OpenGL
(no GLFW, no GLX). It decides *what* to draw and *where*, and *what an input means*
(as an intent). It knows menus, tabs, dialogs, themes, layout, and shortcuts, but it
never knows what "OpenFile" or "a 3D model" or "a workspace" *is* - a host supplies
that domain on top through public seams.

This directory is the developer reference. Each page documents one subsystem: the
public types, their real signatures, and how they fit together.

## Architecture: two roles + a thin demo

The code lives in two include roots with a strict one-way dependency law (`ui -> common`):

- `include/common/` (`Common::`) - cross-cutting primitives that belong to no layer
  (`Bit`, `Sanitize`, `loadJson`, `Unicode`, `System`, `fs`, `BackgroundWorker`).
  Depends on nothing.
- `include/ui/` (`Ui::`) - the domain-blind UI framework and its runnable core
  `Ui::Shell`. Depends on `common/` only.

The runnable app is a thin demo (`include/sig.h` + `src/`) that composes a `Ui::Shell`.
A real host builds on the framework the same way and adds its domain on top.

```
+-----------------------------------------------------------+
|  demo app (src/) / a real host                            |  composes Ui::Shell,
|    - registers domain actions, file handlers, hooks       |  adds content surfaces
+-----------------------------------------------------------+
|  Ui::Shell            (runnable core: init + run loop)    |  shell-actions.md
|    Ui::Action registry | event->intent dispatch          |
+-----------------------------------------------------------+
|  Ui::Render chrome     |  Ui::Window coordinator          |  render.md / windowing.md
|    UiLayout/UiRenderer |    WindowManager + popups        |
|    Context (intents)   |    EglContext + platform peers   |
+-----------------------------------------------------------+
|  seams (ui/interface/): IRender IRenderer IEventApp       |  interfaces.md
|                         IWindow IContext IChromeCommands  |
+-----------------------------------------------------------+
|  Ui::Gl backend (bottom IRender impl) | Ui::Res resources |  render.md / resources.md
+-----------------------------------------------------------+
|  Ui:: vocabulary (type/color/config/intent/...)          |  vocabulary.md
+-----------------------------------------------------------+
|  Common:: primitives (depends on nothing)                |  common.md
+-----------------------------------------------------------+
```

## The two-way intent flow

Input and execution are decoupled through a tagged intent vocabulary:

- Intents-out: `Ui::Render::Context` maps clicks/keys to a `Ui::intent_t`
  (a tagged `IntentKind` + payload). See [render.md](render.md), [vocabulary.md](vocabulary.md).
- Intents-in: `Ui::routeIntent(intent, chrome)` is a pure `IntentKind -> command`
  switch over `Ui::IChromeCommands` (emitAction/openPopup/openDialog/switchTab/...).
  `Ui::Shell` privately implements `IChromeCommands`. See [interfaces.md](interfaces.md).

This is why the framework is host-free: the same intent stream can be recorded,
tested headlessly, or executed against a live shell.

What feeds that flow is capability-driven, not type-driven: an element declares
the events it accepts (`EventKind`, a mask), and a `(type, event)` table states
what an accepted event raises (`binding_t`) and what must repaint afterwards
(`RenderScope`). Adding an interactive element is declaring a mask and a table
row rather than editing hit-test, hover and dispatch switches. Not everything
becomes an intent: continuous streams (a slider drag) reach the host through a
hook, and work the framework can finish itself (tab scroll, dock resize) sets
`result.isDirty` instead. See [render.md](render.md).

## Host seams (all `std::function`, no host-services interface)

- Content surfaces: `WindowManager::addContentSurface` / `removeContentSurface` /
  `setActiveContentSurface` / `setContentSurfaceReady` register host-owned child
  render surfaces (e.g. a 3D viewport). The framework drives
  `[main window] + content surfaces` blind through `IWindow` + `IRenderer` + `IEventApp`.
- Dock content: `WindowManager::setDockRows(dockName, rows)` projects host rows
  into a dock the way `TabBar` takes tabs - the framework renders and hit-tests
  `Ui::Res::Dock::row_t` and never learns what a row means. Selection and
  expand/collapse come back as `ActivateRow` / `ToggleRow` intents; a slider
  row's value comes back through `setOnRowValue` instead, because a drag is a
  stream rather than a request. Also `resetDockScroll(dockName)`.
- Shell hooks: `setFrameTasks` / `setOnTick` (per-loop work),
  `setOnTabActivated` / `setOnTabClosed` / `setOnKeyPress` (domain reactions),
  `setFileHandler(extension, fn)` (per-type file loaders for OpenFile),
  `setOnDockRowValue(fn)` (a dock slider moved).
- WindowManager hooks: `setOnDialogClose` / `setOnElementHover` /
  `setDoubleClickConfig` / `setDialogContentResolver` / `setOnRowValue`, plus
  `setCursor(PointerShape)`.
- Actions: `shell.actions().on(key, fn)` binds a domain action to an opaque
  `actionKey` that res JSON (menus/shortcuts) references.
- Resources: `Ui::Res::ResManager` exposes persisted state via getters plus an
  `onPersistChange` hook and a `registerPersisted(section, key, get, set)` registry
  a host extends with its own keys.

## Minimal host

```cpp
#include "ui/shell.h"

int main()
{
    Ui::Shell shell;

    // Domain action bound to an opaque key referenced by res JSON (menus/shortcuts).
    shell.actions().on("MyAction", [](const std::string & arg) { /* ... */ });

    // Per-extension file loader for the built-in OpenFile action.
    shell.setFileHandler("step", [](const std::string & path) { return loadStep(path); });

    // Domain reactions.
    shell.setOnTabActivated([](Ui::id_t tabId) { /* ... */ });

    shell.initialize();
    shell.run();
}
```

See [shell-actions.md](shell-actions.md) for the full host walkthrough.

## Documentation map

| Page | Covers |
|------|--------|
| [common.md](common.md) | `Common::` primitives (bit, json, sanitize, unicode, fs, system, backgroundworker) |
| [vocabulary.md](vocabulary.md) | `Ui::` value types (type, color, config, convert, registry, index, tabbar, intent/result, ...) |
| [interfaces.md](interfaces.md) | Seams: IRender, IEventApp, IRenderer, IChromeCommands, IWindow, IEventOS, IContext |
| [render.md](render.md) | Render chrome (UiLayout/UiRenderer/Context/DockColumn/popups), the input model, thumb widgets, text fitting + GL backend (GlRender/Rounded/Svg/Font) |
| [windowing.md](windowing.md) | WindowManager, Connector, EglContext, events, pointer shapes, content surfaces, platform peers, pub/sub |
| [resources.md](resources.md) | ResManager + sub-stores + value types + key enums + session.json v2 |
| [shell-actions.md](shell-actions.md) | Ui::Shell, the action subsystem, IO, and building a host |

## Conventions worth knowing

- Header-only in `include/` (`*.cpp` only in `src/`); one type per header.
- `_t` suffix + `alignas` on structs; booleans are `is*`/`has*`.
- All rounded shapes go through `Rounded` (the only SDF/corner path) - see [render.md](render.md).
- EGL, not GLX; the OpenGL context is created via EGL directly.
- `res/*` JSON is the single source of UI values; every string ingest is sanitized
  at parse (`Common::Sanitize`). See [resources.md](resources.md).
- Runtime platform detection: `WAYLAND_DISPLAY` selects Wayland vs X11.

## Building

- `./build.sh <dev|rel|test|ut|ct|cov>` - build (`test`/`ut`/`ct` enable `BUILD_TESTING`;
  `ut`/`ct` build one suite each, `test` both).
- `./run.sh <dev|rel|test|ut|ct> [args...]` - run the demo app, both suites, or one.

See the repo root `CLAUDE.md` for the full contributor guide.
