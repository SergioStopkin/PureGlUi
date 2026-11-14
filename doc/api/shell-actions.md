# ui/shell + ui/action + ui/io - runnable core, actions, IO (Ui::, Ui::Action, Ui::Io)

`Ui::Shell` is the runnable framework core: it owns the whole fw object graph and
runs standalone with no host code - `Ui::Shell shell; shell.initialize(); shell.run();`.
This document covers the shell, the action-dispatch subsystem, the native file IO,
and how the demo app (and a real host) build on top of the same `Shell`.

## Ui::Shell

Header: `include/ui/shell.h`. Namespace `Ui`. `class Shell final : public Ui::IChromeCommands, private Common::NonCopyable`.

Domain-blind coordinator. It owns four members and wires them together:

- `Ui::Res::ResManager m_resManager` - the resource facade (chrome data).
- `Ui::Window::WindowManager m_windowManager` - windowing/event/render coordinator (constructed with `m_resManager`).
- `Ui::Render::Context m_context` - intents-out: maps clicks/keys to `Ui::intent_t` (constructed with `m_resManager`).
- `Ui::Action::Registry m_actions` - the opaque `actionKey -> Ui::action_fn_t` dispatch table.

Shell publicly implements `Ui::IChromeCommands` (the intents-in seam), the
execution target for its own intents. An action can call a command directly, as
`CycleTab` calls `setActiveTab`.

### How Shell fits the whole

Input flows one direction through three seams:

1. An OS event (click/key) reaches Shell via `WindowManager` pub/sub subscriptions (`wireEvents`).
2. Shell asks `Ui::Render::Context` (intents-out; see [render.md](render.md)) to map it: `m_context.mapClick(...)` / `m_context.mapKey(...)` return a `Ui::result_t` carrying `Ui::intent_t`s.
3. Each intent goes to `execute(intent)`, which calls the pure `Ui::routeIntent(intent, *this)` switch (see [interfaces.md](interfaces.md)); that dispatches to Shell's `IChromeCommands` overrides (`emitAction`/`openPopup`/`closePopup`/`openDialog`/`setActiveTab`/`closeTab`/`copyText`/`activateRow`/`toggleRow`/`openRowMenu` + `isPopupOpen`), all private but `setActiveTab`, which `CycleTab` calls.
4. `emitAction(actionKey, arg)` forwards to `m_actions.dispatch(actionKey, arg)` - the action subsystem below.

`run()` is the event loop driving all of the above.

### Accessors

All are `[[nodiscard]]`; all but `fileDialog` have const + non-const overloads.

```cpp
Ui::Res::ResManager &       resManager();
Ui::Window::WindowManager & windowManager();
Ui::Render::Context &       context();
Ui::Action::Registry &      actions();      // host registers domain actions here
Ui::Io::FileDialog &        fileDialog();   // OpenFile's dialog; run() asks it at the top of each pass
```

On macOS, including `ui/shell.h` forces the includer to compile as Objective-C++:
it reaches `<Cocoa/Cocoa.h>` through `io/filedialog.h`, for the `FileDialog` it
owns. Mark any such translation unit
`set_source_files_properties(<file> PROPERTIES LANGUAGE OBJCXX)` (see the demo's
`src/CMakeLists.txt`).

### Lifecycle

```cpp
int  runApp(const Ui::init_hooks_t & hooks = {}); // [[nodiscard]] whole lifecycle
bool initialize(const Ui::init_hooks_t & hooks = {}); // [[nodiscard]]
void run();
void requestStop();   // -> windowManager().stop()
void shutdown();      // -> windowManager().shutdown()
```

`Ui::init_hooks_t` (`ui/inithooks.h`) - the host steps, all optional, named by
purpose rather than spine position because the first two run on both paths:

| hook | when |
|---|---|
| `loadDomainResources` | after framework res load - startup AND every reload |
| `gateFeatures` | after that, before the disable pass - startup AND every reload |
| `afterWindowCreated` | one-time: window + GL exist, renderer does not |
| `afterInit` | one-time: the spine is complete, everything is live |
| `afterReload` | once per reload, after the chrome is back up |
| `beforeExitSave` | one-time, `runApp` only: the loop has ended and the exit session save is next |

- `initialize(hooks)` runs the whole init spine and returns false if window
  creation fails. Spine order: `loadAllResources()` (framework res, then the
  `loadDomainResources` hook) -> `initWindow()` -> `mainWindow().makeCurrent()` ->
  `afterWindowCreated` hook -> `gateAndDisableUnhandled()` (the `gateFeatures`
  hook, then the disable pass) -> `preloadButtonIcons()` -> `initRenderer()` ->
  `wireEvents()` -> `afterInit` hook. A host restores persisted state BEFORE
  calling `initialize()`, and must register its domain actions before it too - the
  disable pass runs inside the spine.
- `runApp(hooks)` is the whole lifecycle in one call: signal handlers, session
  restore, `initialize`, `run`, the `beforeExitSave` hook, session save, exit code.
  The hook is where a host that writes the session off-thread lets that write
  finish: landing after the exit save, its older snapshot would overwrite the
  newest. It deliberately does NOT
  shut down (a host's content surfaces must die before the main window) and does
  not print a banner (only `main()` brackets the object's lifetime).
- `run()` loops while `windowManager().isRunning()`: drains `frameTasks`, takes the open
  dialog's picks (`takePickedFiles`, before that pass's events), polls +
  dispatches all pending OS events, then on any events runs popup-move tracking,
  temp-status expiry, the `onTick` hook, and deferred actions; renders the dirty
  set; and, idle, sleeps until one refresh of the display (`g_config.refreshPeriod()`)
  after the pass began - so a frame a host asks for from `frameTasks` (an animation)
  comes at the display's pace and no faster, and an idle loop polls once a refresh.
  The sleep is `Common::Time::sleepUntil`'s, since a plain one on Windows wakes on the
  15.6 ms timer tick and at 64 Hz paced frames at one or two ticks.
  With `PUREGLUI_STOP_AFTER_FRAME` set (any value), the loop raises SIGINT on itself
  at the end of its first pass, which draws the frame `initialize()` asked for -
  in its render step or in an event's own (a resize draws at once): the same
  handler and shutdown as Ctrl+C, with no clock to guess. CI's ASan step (both repos) runs the app this way, under `xvfb-run`, so the
  run ends the same however slow the machine.
  With `PUREGLUI_SCALE` set to a number from 0.25 to 8, the DPI the OS gives is multiplied by it, as
  Qt's `QT_SCALE_FACTOR` does - for a desktop that sizes the UI wrong, and for CI, whose
  component tests run at 1.25x on GNU/Linux and Windows. A change of the OS's DPI still
  comes through it (`ScaleChange`, laid out again by `reloadChrome()`).
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
void setOnDockRowValue(std::function<void(id_t rowId, fpx_t ratio)> fn);     // a dock slider row moved
void setOnTabMoved(std::function<void(id_t tabId, id_t beforeId)> fn);       // a tab dragged along the strip, a call per jump
```

`setOnDockRowValue` forwards to `WindowManager::setOnRowValue`; it is here
because it is the seam a host actually calls. A slider's value arrives as a hook
rather than an intent because a drag is a continuous stream, not a discrete
request - see [windowing.md](windowing.md) for the dock-content API it pairs
with (`setDockRows` / `resetDockScroll`).

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
void      showTempStatus(const std::string & text);            // show one - resolved text, the caller does the locale lookup
void      setStatusText(const std::string & text);             // the real status line; during a temp message it waits underneath
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
  window's live GL context, tagged `(hardware)`/`(software)`) and `%DISPLAYS%` (every
  display, one line each: `displaysTextOf(NativeWindow::displaysOf(...))`, asked again
  each time so one plugged in since is listed) in dialog content.
  Installed as the dialog content resolver during `wireEvents()`.

#### Menu bar behaviour

The menu bar behaves as every OS's and toolkit's own does - macOS, Windows, GNOME, Qt,
a browser's - since that is what a user's hand already knows. What differs from them is
a defect, not a style:

- A title opens its list on the **press**, not the release. Opening runs nothing, so
  there is nothing for a release to confirm: the list shows as the button goes down,
  and a click made while the pointer sweeps on - its release landing on the next title,
  or past it - still opens what was pressed. Push buttons, toolbar buttons and menu
  items stay on the release, which lets a user slide off one and change their mind.
- A quick click on a title leaves its list open.
- A press on the open list's title closes it; the release that follows does nothing.
- With a list open, the list follows the pointer across the titles, however fast it
  moves (the hold-off below).
- An open list closes when another app takes the focus (`FocusLost`), and one the
  pointer was switching to never opens.
- Not yet: releasing over an item after dragging down from the title, which picks the
  item everywhere else. The OS hands that release to the window the press began in -
  the main window - so the list never sees it.

`TestMenuBar` (component suite) drives these through the shell's own loop, the pointer's
presses, moves and releases put in the OS's event queue as the OS would.

`createMenuPopup(id_t menuId)` is the internal popup builder (private; reached via
the `IChromeCommands::openPopup` override and menu-hover switching), listed here
because the task references it - it is not part of the public host API. Hover switching
holds off while a list was just made, until a frame has drawn it, and while a switch
waits for its frame; a hover that arrives then is held (`m_heldMenuId`) and replayed at
the end of the loop's pass once nothing holds it, if the pointer is still on that button
(`WindowManager::isPointerIn`) - a fast pointer that stops on a button sends nothing more.
It anchors under the bar button (its layout bound) and hands off to `showMenuPopup(menu, anchorXCss, anchorYCss)`,
which the `openRowMenu` override shares: a dock `Menu` row's menu opens right-aligned
under its row (above it when it would run past the window bottom). A row menu is
never restored by key - a reload, move or resize closes it - bar hover does not trade
it for a dropdown, and a press on its own row closes it without reopening.

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
`"OpenFile"`, `"CycleTab"`. Called for you from `Shell::wireEvents()`.

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
| `OpenTabSwitcher` | `action/opentabswitcher.h` | `openTabSwitcher` | `Shell::openTabSwitcher(delta)`: the Ctrl+Tab list of the tabs most recent first (`TabBar::recentIds`, a `Ui::TabSwitch`), on the tab viewed before the active one or the least recent. Up once Ctrl has been held past `input.json` `tabSwitcher.delayMs`; while open its keys come before every other (Ctrl+Tab and Ctrl+Down step down, Ctrl+Shift+Tab and Ctrl+Up up, wrapping; Ctrl+Return switches; Escape takes it down; any other does nothing, so no shortcut behind it runs). Ctrl going up (a key release named by `window/iscontrolkey.h`) or a click on a row - a click on its tab - switches through `setActiveTab`; a quick tap switches with no list. A press off it, the app losing the focus or Escape takes it down unswitched; a key with Ctrl up switches first, its release gone unseen. Not over a dialog; an open menu closes | `next` or `prior` (`res/key/tabstep.h`); any other does nothing |
| `CycleTab` | `action/cycletab.h` | `cycleTab` | the tab right of the active one (`TabBar::idAfter`, the first after the last) or left of it (`idBefore`, the last before the first), through `host.setActiveTab` - as a click on it, the strip scrolled to bring it into view (`TabBar::reveal`); nothing with one tab or none active | `next` or `prior` (`res/key/tabstep.h`); any other does nothing |

Notes:

- A host overrides any key by re-registering it (`shell.actions().on("Reload", ...)`)
  to also reload domain resources.
- `openFile` calls `host.closePopupMenu()` and opens the Shell's `Ui::Io::FileDialog`
  (`host.fileDialog().open`) with the main window as owner and `resManager().lastOpenDir()` /
  `openFileTitle()` / `openFileFilters()`, then returns. Each pass of `Shell::run` takes what the
  dialog picked once it has closed and hands it to `openPickedFiles` (`action/openpickedfiles.h`,
  apart from `openfile.h` so it needs no native dialog to be tested), which
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

Header: `include/ui/io/filedialog.h`. `class FileDialog final`, non-copyable; the Shell
owns one.

```cpp
void open(const Ui::IWindow & owner,
          const std::string & startDir,
          const std::string & title,
          const std::vector<file_filter_t> & filters);
std::optional<std::vector<std::string>> takeFiles();
```

`open` while one is up does nothing. `takeFiles` never waits: the files once the dialog
has closed (empty when cancelled), `std::nullopt` while it is open or none was. Win32's
and macOS's dialogs run their own loop inside `open`, which keeps the window answering.
GNU/Linux's runs beside the loop, which keeps answering the compositor's ping while it is
open - waited on, a Wayland desktop took the app for hung: first the desktop's own through
xdg-desktop-portal's FileChooser (`Ui::Io::Platform::PortalFileChooser`, `include/ui/io/platform/portalfilechooser.h`:
`OpenFile` over D-Bus with GDBus, its `Response` subscribed to before the call on the path its
`handle_token` names, and GLib's default main context turned once a pass by `takeFiles` while
it is asked for), else, where no portal answers the call, zenity or kdialog
(`Ui::Io::Platform::ChildProcess`, `include/ui/io/platform/childprocess.h`: started from its argv with no shell,
its stdout read and its exit checked without blocking). Every GLib object it owns is a
`Common::c_ptr_t<T, Free>` (`include/common/cfree.h`), a `std::unique_ptr` freed with its
type's own function. A portal that has not answered `OpenFile` within 1 s
(`REPLY_TIMEOUT_MS`; a portal started cold answers in about 0.1 s) counts as none: the call is
withdrawn with `Request.Close`, so its dialog closes as it comes rather than beside zenity, and
a dialog still up as the app quits is closed the same way. `open` returns at once, except
where there is a display but no session bus address (`ssh -X`): there GLib runs `dbus-launch`
to find a bus, synchronously, inside `g_bus_get`.

Native multi-select open dialog. Domain-blind: title and filters come from the
caller; empty filters => any file (a trailing "All files" is always appended).
`owner` is the window the dialog is modal to and stands above (`OpenFile` passes the
main window). Win32 reads its handle: it stacks a dialog by its owner, and one with none
went behind the main window's popups. A popup there is owned by its parent window and
never topmost, so a dialog that window owns stands above it. The portal reads its
`windowIdentifier()` - `x11:<XID>`, or on Wayland `wayland:<handle>` from the xdg-foreign
export the main window makes once configured - and parents its dialog to it, modal, so a
click on the main window can never put the dialog behind it. A compositor with no
xdg-foreign exporter leaves the identifier empty and the dialog without a parent.
Under X11 the desktop holds the focus back from it: the portal's dialog carries the time of
the last input the portal saw, older than the Ctrl+O or click that asked for it, which GNOME
takes for the user working elsewhere. So while `FileDialog::isOpen()`, `Shell` has the event
peer watch for it (`WindowManager::setDialogAwaited`), and `X11Event` sends
`_NET_ACTIVE_WINDOW` for a window another program made transient for the main window, timed
by our latest key or button - honoured unless the user has gone elsewhere since.
A `Response` of `Ended` (`Ui::Io::Platform::PortalResponse`, `include/ui/io/platform/portalresponse.h`)
gives no files, as a cancel, and a log line: the portal answers so for a dialog closed by Esc or
its close button as for a backend that fails to show it, and an Esc must never bring up another
dialog. Where no portal answers, `Ui::Io::Platform::dialogCommandsOf(parentWindow, startDir, title, filters)`
(`include/ui/io/platform/dialogcommandsof.h`) gives zenity's and kdialog's command, each a
`Ui::Io::Platform::command_t` (`include/ui/io/platform/command.h`: argv, and NAME=value variables its own over
ours, which `ChildProcess::start` lays over `Common::System::environment()`). Under an
`x11:` parent both are attached to the window - zenity `--modal --attach=<id>` as an X11
client (`GDK_BACKEND=x11`), kdialog `--attach=<id>` (`QT_QPA_PLATFORM=xcb`) - since on a
Wayland desktop only a client of the same X server can reach an XWayland window.
Backends: Win32 `IFileOpenDialog`, macOS `NSOpenPanel`, GNU/Linux the portal, else `zenity`,
else `kdialog` where zenity cannot be started or fails to run - ends with neither a pick (0) nor
a cancel (1), or by a signal (a cancelled one is the user's answer).
Picks are absolute paths; none on cancel or when no dialog tool is available on
GNU/Linux.

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
