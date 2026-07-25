# ui/shell + ui/action + ui/io - runnable core, actions, IO (Ui::, Ui::Action, Ui::Io)

`Ui::Shell` is the runnable framework core: it owns the whole fw object graph and
runs standalone with no host code - `Ui::Shell shell; shell.initialize(); shell.run();`.
This document covers the shell, the action-dispatch subsystem, the native file IO,
and how the demo app (and a real host) build on top of the same `Shell`.

## Ui::Shell

Header: `include/ui/shell.h`. Namespace `Ui`. `class Shell final : private Common::NonCopyable, private Ui::IChromeCommands`.

Domain-blind coordinator. It owns four members and wires them together:

- `Ui::Res::ResManager m_resManager` - the resource facade (chrome data).
- `Ui::Window::WindowManager m_windowManager` - windowing/event/render coordinator (constructed with `m_resManager`).
- `Ui::Render::Context m_context` - intents-out: maps clicks/keys to `Ui::intent_t` (constructed with `m_resManager`).
- `Ui::Action::Registry m_actions` - the opaque `actionKey -> Ui::action_fn_t` dispatch table.

Shell privately implements `Ui::IChromeCommands` (the intents-in seam). It does
not expose that interface; it is the execution target for its own intents.

### How Shell fits the whole

Input flows one direction through three seams:

1. An OS event (click/key) reaches Shell via `WindowManager` pub/sub subscriptions (`wireEvents`).
2. Shell asks `Ui::Render::Context` (intents-out; see [render.md](render.md)) to map it: `m_context.mapClick(...)` / `m_context.mapKey(...)` return a `Ui::result_t` carrying `Ui::intent_t`s.
3. Each intent goes to `execute(intent)`, which calls the pure `Ui::routeIntent(intent, *this)` switch (see [interfaces.md](interfaces.md)); that dispatches to Shell's private `IChromeCommands` overrides (`emitAction`/`openPopup`/`closePopup`/`openDialog`/`switchTab`/`closeTab`/`copyText` + `isPopupOpen`).
4. `emitAction(actionKey, arg)` forwards to `m_actions.dispatch(actionKey, arg)` - the action subsystem below.

`run()` is the event loop driving all of the above.

### Accessors

All are `[[nodiscard]]` with const + non-const overloads.

```cpp
Ui::Res::ResManager &       resManager();
Ui::Window::WindowManager & windowManager();
Ui::Render::Context &       context();
Ui::Action::Registry &      actions();   // host registers domain actions here
```

### Lifecycle

```cpp
bool initialize(const Ui::init_hooks_t & hooks = {}); // [[nodiscard]]
void run();
void requestStop();   // -> windowManager().stop()
void shutdown();      // -> windowManager().shutdown()
```

- `initialize(hooks)` runs the whole init spine and returns false if window
  creation fails. Spine order: `loadAllResources()` (framework res, then the
  `loadDomainResources` hook) -> `initWindow()` -> `mainWindow().makeCurrent()` ->
  `afterWindowCreated` hook -> `gateAndDisableUnhandled()` (the `gateFeatures`
  hook, then the disable pass) -> `preloadButtonIcons()` -> `initRenderer()` ->
  `wireEvents()`. A host restores persisted state BEFORE calling `initialize()`,
  and must register its domain actions before it too - the disable pass runs
  inside the spine.
- `run()` loops while `windowManager().isRunning()`: drains `frameTasks`, polls +
  dispatches all pending OS events, then on any events runs popup-move tracking,
  temp-status expiry, the `onTick` hook, and deferred actions; renders the dirty
  set; and sleeps ~16 ms when idle.
- `requestStop()` / `shutdown()` stop the loop and tear down the window manager.

### The load cycle

`initialize()` and `reloadChrome()` share one ordering, so startup and reload can
never produce different chrome:

```cpp
void loadAllResources();        // resManager().loadAll(), then the loadDomainResources hook
void gateAndDisableUnhandled(); // the gateFeatures hook, then disableUnhandledItems()
```

Both are private. The disable pass runs LAST because it also collapses a parent
whose every child ended up disabled, so it must observe every `enabled` write the
gate made; reversing the two yields different chrome from identical state.

The remaining spine steps (`initWindow`, `preloadButtonIcons`, `initRenderer`,
`wireEvents`) are framework-internal - `initialize()` owns their order. `Shell`'s
constructor binds the built-in actions via `Ui::Action::registerActions(*this)`, so
the registry is complete before a host registers anything: a host overrides a
built-in key simply by calling `actions().on()` later, with no ordering rule.

`disableUnhandledItems()` stays public, but only for registering an action
LATE (after `initialize()` returned); both entry points already run it. It greys
every leaf menu item whose `actionKey` has no registered handler (`m_actions.has(key)`),
so the chrome never offers a dead click. Dialog/submenu items stay enabled (the
shell drives them without a registry entry).

### Host hooks

Domain-blind seams; all empty on a standalone shell.

```cpp
void setFrameTasks(Ui::task_fn_t fn);              // run once at the top of every loop iteration
void setOnTick(Ui::task_fn_t fn);                  // run after an event batch is processed
void setOnTabActivated(std::function<void(id_t)> fn);
void setOnTabClosed(std::function<void(id_t)> fn);
void setOnKeyPress(Ui::predicate_fn_t fn);         // consume domain shortcuts before intent mapping (return true = handled)
void setFileHandler(const std::string & extension, Ui::action_fn_t handler); // per-extension loader (lowercase, no dot)
```

Related file-handler helper:

```cpp
bool runFileHandler(const std::string & extension, const std::string & file) const;
```

Returns false (no-op) when no handler is registered for `extension`, letting
`OpenFile` show the no-handler warning dialog. (`Ui::task_fn_t = std::function<void()>`,
`Ui::predicate_fn_t = std::function<bool(const std::string &)>`,
`Ui::action_fn_t = std::function<void(const std::string &)>` - all from `ui/type.h`.)

### Chrome / popup-menu lifecycle

Public interactive-chrome orchestration (menus, popups, dialogs, temp status):

```cpp
void      reloadChrome();                                      // save open chrome, replay the load cycle, reopen
Ui::key_t activeMenuKey() const;                               // hierarchical key of deepest active node ("" = none)
void      restoreActiveMenu(const Ui::key_t & activeKey);      // reopen dropdown/submenu named by key + re-highlight leaf
void      reopenActivePopup();                                 // recreate open popup at its anchor (after move/resize)
void      closePopupMenu();                                    // close any open popup (no-op if none)
bool      hasTempStatus() const;                               // true while an auto-expiring status message shows
std::wstring resolveDialogPlaceholders(const std::string & tpl);
```

- `reloadChrome()` remembers any open dialog/menu (as a hierarchical
  key that survives id reassignment), closes it, runs the host-supplied
  `reloadResources` step, re-queries display DPI, and reopens what was open. The
  save/close/reopen is generic; the host supplies only the domain reload body.
- `reopenActivePopup()` mirrors that menu-state preservation across a window
  move/resize.
- `closePopupMenu()` is called before opening a native modal (e.g. the file dialog).
- `resolveDialogPlaceholders(tpl)` expands `%VERSION%` (from `APP_VERSION`),
  `%CPU%` and `%RAM%` (from `Common::System`), and `%GPU%` / `%GL%` (from the main
  window's live GL context, tagged `(hardware)`/`(software)`) in dialog content.
  Installed as the dialog content resolver during `wireEvents()`.

`createMenuPopup(id_t menuId)` is the internal popup builder (private; reached via
the `IChromeCommands::openPopup` override and menu-hover switching), listed here
because the task references it - it is not part of the public host API.

## Action subsystem (Ui::Action)

The action model has no runtime name->function reflection. An action is an opaque
string key paired with a callback; the binding of key to function is an explicit
list (`actionmap.h`).

### Ui::Action::Registry

Header: `include/ui/action/registry.h`. `class Registry final`.

The `actionKey -> Ui::action_fn_t` dispatch table
(`Ui::action_fn_t = std::function<void(const std::string & arg)>`). The framework
owns the mechanism; the host fills it. It stays domain-blind - it never knows what
an action does.

```cpp
void on(std::string actionKey, Ui::action_fn_t handler);                    // register/overwrite
void dispatch(const std::string & actionKey, const std::string & arg = "") const; // invoke if present, else no-op
bool has(const std::string & actionKey) const;                             // [[nodiscard]]
```

One entry per action: parameterized actions read `arg` (e.g. the item label a menu
click carries), simple ones ignore it. There is no value-provider seam on the
Registry. The `"action"` key itself comes from res JSON (menus/shortcuts); a menu
item or toolbar button whose key has no handler is greyed out by `Shell::disableUnhandledItems`.
A host registers a domain action via `shell.actions().on(key, fn)`.

### Ui::Action::registerActions

Header: `include/ui/action/actionmap.h`.

```cpp
template <class Host>
void registerActions(Host & host);
```

Binds every built-in action into `host.actions()`, pairing each res-JSON `"action"`
key with its implementation. It is a plain iterate-once `std::to_array` of
`{key, function-pointer}` pairs (not a second map - the live map is the Registry it
seeds), templated on `Host` so `Ui::Action` never depends on the concrete shell
type. Bound keys: `"ExitApp"`, `"Reload"`, `"SwitchThemeMode"`, `"SwitchTheme"`,
`"OpenFile"`. Called for you from `Shell::wireEvents()`.

## Built-in actions

Each built-in action is a host-templated free function in `Ui::Action`, one per
header, with the uniform signature `template <class Host> void name(Host & host, const std::string & arg)`.
Being templated on `Host` keeps `Ui::Action` independent of `Ui::Shell`; the body
is type-checked when bound.

| Key | Header | Function | Intent | arg |
| --- | --- | --- | --- | --- |
| `ExitApp` | `action/exitapp.h` | `exitApp` | `host.windowManager().stop()` - end the event loop | ignored |
| `Reload` | `action/reload.h` | `reload` | `host.reloadChrome()` - replays the load cycle (framework res + the host's hooks), disable pass, `apply(changed())`, `requestContentRefresh` | ignored |
| `SwitchThemeMode` | `action/switchthememode.h` | `switchThemeMode` | toggle dark/light (`resManager().switchThemeMode()`), re-apply diff, refresh | ignored |
| `SwitchTheme` | `action/switchtheme.h` | `switchTheme` | select theme by name; no-op if `arg` empty or already active | theme name |
| `OpenFile` | `action/openfile.h` | `openFile` | native open dialog, then route each picked file to its per-extension handler | ignored |

Notes:

- A host overrides any key by re-registering it (`shell.actions().on("Reload", ...)`)
  to also reload domain resources.
- `openFile` calls `host.closePopupMenu()`, invokes `Ui::Io::FileDialog::openFiles`
  with `resManager().lastOpenDir()` / `openFileTitle()` / `openFileFilters()`,
  updates `lastOpenDir` from the last pick, then for each file computes
  `fileExtension(file)` and calls `host.runFileHandler(extension, file)`. If no
  handler is registered it calls `openNoHandlerDialog(host, extension)` - a
  Warning-typed dialog reading `"No handler for *.<ext>"` (prefix localized via
  `DialogNoHandlerContent`, icon from the Warning icon role) - and stops.

### Ui::Action::fileExtension

Header: `include/ui/action/fileextension.h`.

```cpp
inline std::string fileExtension(const std::string & file);
```

Returns the lowercase file extension without the dot (`"/p/Model.STEP" -> "step"`).
This is the dispatch key `OpenFile` uses to find a handler. Pure `std::filesystem`,
deliberately free of the native file-dialog header (which drags Cocoa on macOS),
so it is unit-testable without GL/ObjC.

## IO (Ui::Io)

### Ui::Io::file_filter_t

Header: `include/ui/io/filefilter.h`. `struct alignas(64) file_filter_t final`.

One file-type filter for the native open dialog. Domain-blind - the host supplies
the set (loaded from res JSON), so the framework hardcodes no extensions.

```cpp
std::string name; // human-readable, e.g. "CAD files (STEP, IGES)"
std::string spec; // canonical semicolon-separated globs: "*.step;*.stp;*.iges"
```

Each platform backend reshapes `spec` as needed (Win32 uses it verbatim, zenity
wants spaces, macOS wants bare extensions).

### Ui::Io::FileDialog

Header: `include/ui/io/filedialog.h`. `class FileDialog final` (all-static; not
constructible).

```cpp
static std::vector<std::string> openFiles(const std::string & startDir,
                                          const std::string & title,
                                          const std::vector<file_filter_t> & filters);
```

Native multi-select open dialog. Domain-blind: title and filters come from the
caller; empty filters => any file (a trailing "All files" is always appended).
Backends: Win32 `IFileOpenDialog`, macOS `NSOpenPanel`, Linux `zenity` then
`kdialog` fallback. Returns the selected absolute paths (empty on cancel or when
no dialog tool is available on Linux).

## Demo app

The runnable demo proves `include/ui/` is a complete framework: it composes a
`Ui::Shell` with no domain content, so the window opens and all chrome works while
the central content region stays the theme background.

- `include/sig.h` - host-agnostic signal handling, free of any framework type:
  - `void g_app_init(std::function<void()> stopCallback) noexcept;` - stores the stop callback.
  - `std::string_view signal_name(int sig) noexcept;`
  - `void on_signal(int sig) noexcept;` - flips the running flag via the stored callback (first signal graceful, second forces `_Exit`).
- `src/sig.cpp` - the implementation (an internal `std::function<void()> g_stop`).
- `src/main.cpp` - the entry point. It:
  1. constructs `Ui::Shell shell;`
  2. wires signals: `g_app_init([&shell]{ shell.requestStop(); });` then `std::signal(SIGINT/SIGTERM, on_signal)`
  3. restores the persisted session, then calls `shell.initialize()`
  4. `shell.run();`
  5. on clean exit saves the session and calls `shell.shutdown()`

Session persistence is the one host touch: the demo restores window geometry +
theme (+ dock state) via `resManager().deserializeSession(blob)` and saves via
`resManager().serializeSession()`. The blob is opaque to the app - `ResManager`
owns the format - so `main.cpp` does only plain file I/O.

A real host builds on `Ui::Shell` the same way and adds its domain on top of the
same seams: content surfaces (`shell.windowManager().addContentSurface(...)`),
domain actions (`shell.actions().on(key, fn)`), per-type file loaders
(`shell.setFileHandler(ext, fn)`), and domain reactions
(`setOnTabActivated`/`setOnTabClosed`/`setOnKeyPress`/`setFrameTasks`/`setOnTick`).

## Getting started: building a host

A minimal host that adds one domain action, one file handler, and a tab hook:

```cpp
#include "sig.h"
#include "ui/shell.h"

#include <csignal>
#include <iostream>
#include <string>

int main()
{
    Ui::Shell shell;

    // Stop the loop on SIGINT/SIGTERM.
    g_app_init([&shell]() { shell.requestStop(); });
    (void)std::signal(SIGINT, on_signal);
    (void)std::signal(SIGTERM, on_signal);

    // Bring up the framework (loads resources, creates window + renderer, wires
    // events, binds the built-in actions).
    if (!shell.initialize()) {
        std::cerr << "Failed to initialize Shell" << std::endl;
        return 1;
    }

    // Register a domain action. Its key must match an "action" value in res JSON
    // (a menu/shortcut) to be reachable from the UI. arg is the item label for
    // parameterized actions, "" otherwise.
    shell.actions().on("MyDomainAction", [&shell](const std::string & arg) {
        std::cout << "MyDomainAction fired: " << arg << std::endl;
    });

    // Register a loader for a file extension (lowercase, no dot). OpenFile routes
    // picked files of this type here; types with no handler show a warning dialog.
    shell.setFileHandler("step", [&shell](const std::string & file) {
        std::cout << "Load STEP model: " << file << std::endl;
        // ... build the domain workspace, add a content surface, etc.
    });

    // React when the user activates a tab (open the matching workspace, etc.).
    shell.setOnTabActivated([&shell](Ui::id_t tabId) {
        std::cout << "Tab activated: " << tabId << std::endl;
    });

    shell.run();
    shell.shutdown();
    return 0;
}
```

Notes:

- Register actions BEFORE `initialize()` so their menu items are not greyed out:
  the disable pass runs inside the spine. Registering later works too, but then
  call `disableUnhandledItems()` yourself. Domain steps that must land at a
  specific point of the spine go in `Ui::init_hooks_t` (see the load cycle above),
  never by driving the spine steps directly.
- Overriding a built-in key (e.g. re-registering `"Reload"`) replaces the default
  handler.

## See also

- [interfaces.md](interfaces.md) - `IChromeCommands` (intents-in), `routeIntent`, `IRenderer`/`IEventApp`.
- [resources.md](resources.md) - `ResManager` and the res-JSON that supplies action keys, menus, shortcuts, filters.
- [windowing.md](windowing.md) - `WindowManager`, content surfaces, popups/dialogs.
- [README.md](README.md) - framework overview.
