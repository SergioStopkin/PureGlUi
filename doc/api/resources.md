# ui/res - resource framework (Ui::Res)

The domain-blind resource layer: it parses every `res/*` JSON file into typed
values, resolves display strings through a locale table, and owns the session
persistence format. `Ui::Res::ResManager` is the public facade; it composes 7
logical sub-stores (`Ui::Res::Store`), exposes their state through typed value
structs (`Ui::Res::Type`) plus the dock types (`Ui::Res::Dock`), and single-
sources every JSON key spelling through the key enums (`Ui::Res::Key`).

## Trust boundary and data-driven rules

- `res/` is a trust boundary: hosts and users edit these files. Every string a
  loader ingests is wrapped in `Common::Sanitize` at parse time - `filePath` for
  path/icon fields, `url` for real URLs, `string` for everything else (labels,
  keys, titles, content). Bool and number fields are exempt. Audit rule: any
  `.value("` in a loader is wrapped.
- Theme display names come from the JSON `"name"` field (registered into
  `LocaleManager` and resolved via `get(key)`), NOT from the filename. The
  `<key>-dark.json` / `<key>-light.json` filename stems are only pairing keys.
- For every per-file resource directory (docks, themes, auto-submenus), identity
  is the JSON `"name"` field. Filenames are cosmetic and may carry a leading
  `NN.` ordering prefix; loaders never extract identity from the filename.

## ResManager

Header: `include/ui/res/resmanager.h` (namespace `Ui::Res`).

The facade. GL-free and FreeType-free (pure parse + state; no rendering). It is
neither copyable nor movable: it owns sub-stores that hold references to sibling
members (`MenuStore`, `DockStore` take injected `&` collaborators) and those
stores are `Common::NonCopyable`.

Construction and load:

- `explicit ResManager(std::string resDir = "./res")` - builds the `ResPath`,
  loads `app.json` for identity (`title`) and session location
  (`sessionDir`/`sessionFile`) plus the native open-dialog config, then builds
  the persisted-setting registry.
- `void loadAll()` - resets the `Changed` mask, then loads locale, theme (current
  name+mode), layout+popup, input, dialog, shortcut, icon defaults, buttons,
  menus, docks, and rebuilds the action map. Sub-store loads funnel their
  `Changed` bit through an internal `markChanged` so `changed()` aggregates them.

Read accessors (all return `const &` unless noted):

- `layout()` -> `Type::layout_t`, `popup()` -> `Type::popup_t`,
  `theme()` -> `Type::theme_t`, `input()` -> `Type::input_t`.
- `localeManager()` -> `LocaleManager`, `resPath()` -> `ResPath`.
- `title()`, `openFileTitle()`, `openFileFilters()` -> `vector<Io::file_filter_t>`.
- `tabCloseIcon()`, `tabArrowLeft()`, `tabArrowRight()` - convenience layout icons.
- `shortcuts()` -> `unordered_map<string,string>` (key-combo -> actionKey).
- `iconDefault(const std::string& role)` and
  `iconDefault(Key::IconRoleKey role)` -> `Type::icon_default_t` (empty when the
  role is unknown; the enum overload keeps fw role spellings single-sourced).
- `themePreviewColors(key)` -> `pair<color_pair_t, color_pair_t>` ({dark, light}).
- `dialogTypeConfig(Type::DialogType)` -> `Type::dialog_type_config_t`.
- `buttons()` -> `vector<Type::button_t>`, `menus()` -> `vector<Type::menu_t>`.
- `isThemeDark()`, `themeIcon()`, `themeName()`.

Menu queries and mutation:

- `actionKeyFor(id_t elementId)` -> `std::string` - actionKey for a clicked id.
- `findMenuItem(id_t elementId)` -> `Type::menu_t` - resolve a node by numeric id.
- `findMenuItemByKey(const key_t& key)` -> `Type::menu_t` - resolve by
  hierarchical key ("View:Theme:default"); survives a reload that renumbers ids.
- `isActiveItem(const Type::menu_t&)` -> `bool` - radio highlight (item.label ==
  the action's current value), computed live.
- `setActionEnabled(actionKey, bool)` - enable/disable every item bound to an
  action (recurses into submenus).
- `setMenuItemEnabled(actionKey, label, bool)` - per-(action,label) variant for
  parameterised actions where many items share one actionKey.
- `disableUnhandledItems(const predicate_fn_t& isHandled)` - grey out leaf
  action items with no handler, and parents whose every child is disabled;
  dialog items stay enabled. Runtime equivalent of `"enabled": false`.
- `setActionValueProvider(actionKey, provider_fn_t)` - host wires the value
  getter that drives a stateful action's highlight.

State getters/setters (each setter fires the persist hook where noted):

- `tabBar()` -> `Ui::TabBar&` (mutable and const) - chrome tab view-model.
- `activeMenuId()`, `setActiveMenu(id_t)`, `clearActiveMenu()`.
- `statusText()`, `setStatusText(text)`.
- `changed()` -> `Type::Changed`.
- `switchThemeMode()`, `setThemeName(name)` - reload theme + fire persist hook.
- `dockState(name)` -> `Dock::dock_state_t`; `setDockState(name, state)` (const;
  fires persist hook).
- `lastOpenDir()`, `setLastOpenDir(dir)`.
- `sessionWindowX/Y/Width/Height()`; `setSessionWindowGeometry(x,y,w,h)`
  (diff-checked, fires persist hook only on real change).
- `sessionOpenFiles()`, `sessionActiveFile()`;
  `setSessionOpenFiles(files, activeFile)` (diff-checked).

Persistence seam:

- `registerPersisted(Key::SectionKey section, std::string key,
  provider_fn_t getter, action_fn_t setter, bool skipIfEmpty = false)` - add one
  string-valued persisted setting. Args read outer-to-inner (section then key),
  matching the `j[section][key]` JSON they produce. `skipIfEmpty` omits the key
  from a save when the value is empty. A host extends session.json with its own
  domain keys through this call.
- `setOnPersistChange(task_fn_t)` - the host hook fired whenever a persisted
  setting changes; wire it to a save. Suppressed while a restore is in progress.
- `isRestoringSession()` - true while `deserializeSession` applies values. A host
  whose own state objects fire their own persist hooks (reached through the
  registry setters, which the fw cannot suppress) checks this in its save path.
- `sessionDir()`, `sessionFile()`, `sessionPath()` - storage location, from
  app.json. `sessionPath()` joins the two.
- `loadSession(path)` / `static writeSession(path, blob)` - the file I/O, path
  explicit (pass `sessionPath()` for the default location). Missing file on load =
  fresh start, not an error. The write goes to a sibling temp file and is renamed
  over the target, so a crash or a racing save cannot leave a half-written file.
- `saveSession()` - serialize + write to `sessionPath()` in one call.
- `serializeSession()` -> `std::string` - encode all persisted state to a v2
  session blob (see below).
- `deserializeSession(const std::string&)` - restore from a v2 blob; tolerant of
  missing/garbage data; ingested paths pass `Sanitize`; reloads the theme file
  for the restored name/mode.

Built-in persisted settings registered at construction: `view.theme`,
`view.themeMode`, `paths.lastOpenDir` (skipIfEmpty). Window geometry and the
dock array are written directly by `serializeSession`, not through the string
registry. `SwitchTheme`'s action value provider (themeName) is also wired here.

## ResPath

Header: `include/ui/res/respath.h` (namespace `Ui::Res`).

Resolves bundled `res/` paths relative to a base directory. Joins via
`std::filesystem::path` and returns forward-slash strings (`generic_string()`),
so log output, SVG cache keys, and font registration stay platform-neutral.

- `explicit ResPath(std::string base = "./res")`.
- File resolvers: `file(name)` (anything res-root-relative - what a host uses for
  its own resources), `layoutFile()`, `appFile()`, `inputFile()`,
  `dialogFile()`, `iconDefaultsFile()`, `shortcutFile()`.
- Directory resolvers: `fontDir()`, `buttonDir()`, `menuDir()`, `dockDir()`,
  `submenuRoot()`, `submenuDir(name)`.
- Parameterised: `icon(name)`, `fontFile(name)`, `locale(lang)`,
  `theme(name, mode)` (-> `submenu/theme/<name>-<mode>.json`).

## Util

Header: `include/ui/res/util.h` (namespace `Ui::Res`). Static-only helper class
(deleted ctor/dtor).

- `static std::string strKey(const std::string&)` - remove spaces, lowercase
  (shortcut-key normalization).
- `static std::vector<std::string> splitMenuKey(const std::string&)` - split a
  hierarchical menu key on ':' ("View:Theme:default" ->
  {"View","Theme","default"}); always returns at least one segment.
- `static std::size_t timeKey()` - nanosecond steady-clock key.

## LocaleManager

Header: `include/ui/res/localemanager.h` (namespace `Ui::Res`).

Locale string table keyed by JSON key.

- `bool load(const std::string& locale, const ResPath& res)` - load
  `res/locale/<locale>.json`; every value sanitized as `string`.
- `void set(const std::string& key, const std::string& value)` - register a
  dynamic string (used to register theme/auto-submenu display names from `"name"`).
- `const std::string& get(const std::string& key) const` - lookup; returns an
  empty string when absent.

## Sub-stores (Ui::Res::Store)

The 7 logical loaders `ResManager` composes. Loaders that diff old-vs-new return
a `Type::Changed` bit which the facade ORs into its pending reload mask; loaders
that never diff return `void`. `MenuStore` and `DockStore` are
`Common::NonCopyable` and take injected `&` collaborators.

- `IconStore` (`store/iconstore.h`) - loads `res/icon-defaults.json`: per-role
  default icons with the alias chain resolved to a concrete `.svg`, plus
  placement. `load()` returns `Changed::Icon`. `iconDefault(role)` ->
  `Type::icon_default_t`.
- `DialogStore` (`store/dialogstore.h`) - loads `res/dialog.json`: per
  dialog-type button rows (label + action + primary flag), resolved from a shared
  button-label table. `load()` returns `void`. `dialogTypeConfig(DialogType)` ->
  `Type::dialog_type_config_t`.
- `ShortcutStore` (`store/shortcutstore.h`) - loads `res/shortcut.json`:
  normalized key-combo -> actionKey map. `load()` returns `Changed::Shortcut`.
- `LayoutStore` (`store/layoutstore.h`) - loads `res/layout.json` once, deriving
  BOTH `layout_t` (region geometry, `:root` CSS vars, dialog/tab/dock metrics)
  and `popup_t` (dropdown item/separator heights) from the single parse. Returns
  the combined `Changed::Layout | Changed::Popup`. Dock definitions are committed
  separately via `setDocks()` (`ResManager::loadDocks` supplies them).
- `ThemeStore` (`store/themestore.h`) - loads the active theme
  (`submenu/theme/<name>-<mode>.json` -> `theme_t`), resolves `var()` chains and
  `darker()`/`lighter()` modifiers, and owns the current name/mode. `loadCurrent`
  / `loadTheme` / `setThemeName` / `switchThemeMode` return `Changed::Theme`.
  `scanThemeNames()` enumerates theme keys, registers each display name (from
  JSON `"name"`) into `LocaleManager`, and caches `{cl-main, bg-main}` preview
  colors per variant.
- `MenuStore` (`store/menustore.h`) - the menu/button/action machinery. Loads
  `res/menu/*` and `res/button/*`, parses the recursive `menu_t` tree
  (depth-capped by `layout.menuMaxDepth`), expands `"submenus": {"auto": ...}`
  into one child per file in the named `submenu/` subdir, assigns numeric ids and
  hierarchical keys, and builds the runtime lookups - element-id -> actionKey plus
  node-by-id and node-by-key (`buildIndexes`, run once per load after both
  loaders, since the chrome resolves a node per element per render).
  `loadMenus` / `loadButtons` return `Changed::Menu` / `Changed::Button`. Also
  answers find-by-id/key, radio highlight, and enable/disable queries. Composes `LocaleManager`, `IconStore`, `ThemeStore`,
  `LayoutStore`, `ResPath` by reference. The disable pass lives in
  `store/menudisable.h` (`disableUnhandled`, one overload for menus + one for
  buttons); the auto-submenu walker's intermediate is `submenu_entry_t`
  (`store/submenuentry.h`).
- `DockStore` (`store/dockstore.h`) - session-persisted per-dock state
  (`dock_state_t`), keyed by dock name. Reads the dock CONFIG list from the
  injected `LayoutStore` (for orphan-filtering on write). `writeDockJson` /
  `readDockJson` (de)serialize the session `docks` array; `setDockState` is const
  (mutable storage) so a `const ResManager&` holder can commit dock prefs.
  Dock names are capped at `MAX_DOCK_NAME_LENGTH` (64); widths sanitized to
  finite non-negative (NaN/inf/negative -> 0 = collapsed).

## Value types (Ui::Res::Type)

Headers under `include/ui/res/type/`. All structs use the `_t` suffix and
`alignas()`; all provide `operator==` (used by the stores' change diffing).

Geometry primitives:

- `bound_t` (`bound.h`) - `{x,y,w,h}` fpx rectangle with `contains(px,py)`.
- `border_t` (`border.h`) - four per-corner radii (`topLeft`..`bottomLeft`) with
  `scaled(factor)`, `anyNonZero()`, and a `std::hash` specialization.
- `region_t` (`region.h`) - a laid-out UI region: height/width/margin/padding/
  top/left/right/bottom + a `border_t`.

Visual primitives:

- `color_pair_t` (`colorpair.h`) - `{fg, bg}` `Ui::Color` pair; free `inheritBg`
  helper resolves an inherited background.
- `font_t` + `FontWeight` enum (`font.h`) - `{family, size, weight}`; has a
  `std::hash` specialization.
- `theme_preview_t` (`themepreview.h`) - theme-swatch geometry (border, width,
  height, right offset, splitAngle) for the themes submenu.

Text placement (read from a block's `text-align` / `vertical-align`, so the JSON
spelling matches the CSS property it mirrors):

- `AlignH` enum + `alignHFromName` (`alignh.h`) - `Left`, `Center`, `Right`,
  `CenterClamped`. The last centres while there is room and falls back to
  left-aligned rather than overflowing the left edge - what a tab or a menu
  button needs when its label outgrows the box.
- `AlignV` enum + `alignVFromName` (`alignv.h`) - `Top`, `Center`, `Bottom`.
  `Center` is cap-height centred rather than line-box centred: text looks centred
  to the eye when its capitals are, not when its ascender/descender bounds are
  (see `font_metrics_t::baselineCap`).

Both are parameters of `IRender::drawText`, replacing the old `bool centered`.

Aggregate value blocks:

- `layout_t` (`layout.h`) - the full layout block: region_t members (topMenu,
  toolbars, statusBar, workspace, workspaceTab, dialog, ...), dock defaults,
  theme-preview geometry, hover/active border radii, dialog/tab metrics, the
  `docks` config vector, icon filenames, and scalar `:root` vars
  (`menuMaxDepth`, `windowWidth`/`windowHeight`, icon shadow/scale, etc.).
  Scrollbar metrics are named `scrollbar*` rather than `dialogScrollbar*`: the
  block is shared by the dialog and the docks. `scrollbarHoverW` doubles as the
  HIT width in both states. Slider metrics are `sliderTrackH`, `sliderThumbW/H`
  and `sliderThumbHoverW/H` - only the track's THICKNESS is stated, because its
  length is the row's right column, which layout cannot know without the dock's
  width; the thumb states both sides since neither follows the row.
  `windowMinWidth`/`windowMinHeight` are the CHROME floor only - the size at
  which toolbars, tabs and status stay usable with no dialog open. What a dialog
  needs is added on top by `ResManager`, so a bigger dialog cannot make itself
  unreachable.
- `popup_t` (`popup.h`) - cached dropdown metrics: itemHeight, item padding,
  separator height/margins.
- `theme_t` (`theme.h`) - the full resolved theme: per-element `font_t`s,
  `color_pair_t`s for every region and interactive state (menu, buttons, tabs,
  dialog, scrollbar, status bar), standalone colors (shadow, model, error, info,
  warn, disabled/separator/shortcut), and the embedded `Dock::dock_theme_t`.
  Note `colorError` is also the FALLBACK any unresolved block colour takes: an
  absent block, an absent property and a mistyped value all resolve to it alike,
  so a misspelled key paints the element in the theme's error colour rather than
  leaving it unset. A test asking "is this block defined?" must compare against
  `colorError`, not against alpha.
- `input_t` (`input.h`) - scroll + key-animation tunables from `res/input.json`
  (scrollNatural/Speed/Smooth/SnapThreshold, keyAnimationDelay).

Menu / button / dialog / icon:

- `menu_t` (`menu.h`) - one node in the recursive menu tree (same type at every
  depth): id, order, actionKey, label, visible/enabled/separator flags,
  popupHeight, submenu auto-key + submenuActionKey, shortcut, icon + IconPlace,
  embedded `dialog_t`, child `items`, and the hierarchical `key`.
- `button_t` (`button.h`) - a toolbar button: id, width/height, order, colors,
  actionKey, label, tooltip, enabled/visible, icon, key.
- `dialog_t` + `dialog_button_config_t` + `dialog_type_config_t` +
  `DialogType`/`DialogAction` enums with `*FromName` mappers (`dialog.h`) -
  dialog definition (type, locale keys for title/content/link, icon, file,
  runtime `contentOverride`, size overrides), per-button config (label + action +
  primary), and per-type config (button list).
- `icon_default_t` (`icondefault.h`) - one resolved icon-defaults entry
  (concrete `.svg` + materialized `IconPlace`).
- `IconPlace` enum + `iconPlaceFromName` (`iconplace.h`) - Left / Right icon
  position relative to the label.
- `Changed` enum (`changed.h`) - `uint16_t` bit flags (None, Locale, Theme,
  Layout, Popup, Shortcut, Menu, Button, Render, Icon, All) that stores return
  and the facade ORs.

## Key enums (Ui::Res::Key)

Headers under `include/ui/res/key/`. One `enum class <Domain>Key` per JSON
domain, each with a `<domain>KeyName(key) -> std::string` mapper. This is the
single source of truth for every JSON key spelling the framework references, so
a key can never be spelled two ways. fw-invented keys are camelCase; only real
CSS property/selector spellings stay kebab. Keys only other res JSON references
(menu icon roles, action keys, host-added keys) stay free-form strings - that
set is deliberately open and data-driven.

Domains present:

- `AppKey` (`app.h`) - `app.json`: title, sessionDir, sessionFile, openFile,
  filters, name, spec.
- `DialogKey` (`dialog.h`) - `dialog.json`: buttons, label, types, primary.
- `DockKey` (`dock.h`) - `dock/*.json`: name, anchor, order, defaultWidth.
- `IconKey` (`icon.h`) - `icon-defaults.json` entry fields: icon, place.
- `IconRoleKey` (`iconrole.h`) - fw-referenced icon roles: windowIcon,
  windowIconSymbolic, warning.
- `MenuKey` (`menu.h`) - `menu/*.json` + `button/*.json` node fields (label,
  action, shortcut, icon, separator, enabled, visible, order, items, tooltip,
  width, height, name, submenus, auto, dialog, type, title, content, link, file).
- `SectionKey` (`section.h`) - session.json v2 top-level sections: mainWindow,
  view, paths, files, docks.
- `SessionKey` (`session.h`) - session.json v2 keys: version, lastUpdate, x, y,
  width, height, theme, themeMode, lastOpenDir, active, open, name, memoryX.
- `ElementKey` (`element.h`) - CSS block SELECTORS shared by layout.json and the
  theme files, so both stores key off one spelling.
- `CssPropKey` (`cssprop.h`) - CSS PROPERTY names read from a block.
- `LayoutKey` (`layout.h`) / `ThemeKey` (`theme.h`) - the per-store `:root`
  custom-property variables (`--kebab`), plus the theme's top-level `name`.

`ElementKey`, `CssPropKey`, `LayoutKey` and `ThemeKey` each end in a `Count`
sentinel - the enumerator total, never a selector - so an exhaustive check counts
against the enum instead of a hand-kept number. Each mapper carries a
`case ...::Count: break;` that falls through to the same empty string an
out-of-range value gets, which keeps `-Wswitch` catching a real key that has no
spelling. The two guarantees are worth separating: a missing spelling is a
compile error (the switches have no `default:`), while a spelling shared by two
keys is invisible to the compiler and is what the sentinel-checked test catches.

## Dock (Ui::Res::Dock)

Headers under `include/ui/res/dock/`.

- `DockAnchor` enum + `dockAnchorFromName` / `dockAnchorToName` (`anchor.h`) -
  Left / Right screen edge; further topology is expressed via `order`, not new
  anchors.
- `dock_config_t` (`config.h`) - one dock definition from `res/dock/*.json`:
  name (stable identity + persistence key), anchor, order (1 = innermost),
  defaultWidth (applied on first expand). Whether a dock is expanded is NOT
  configured here - first launch is always collapsed.
- `dock_layout_t` (`layout.h`) - shared dimensional/interaction tunables from
  layout.json applied to every dock: gripWidth, gripRadius, clickThreshold,
  gripIcon, plus the row metrics `rowIndentRatio` / `rowExpanderRatio` /
  `rowKeyRatio`. Those three are fractions OF ROW HEIGHT, so the tree stays
  proportional at any DPI or font size instead of needing a px value per scale.
  No min/max width bounds (floor 0, ceiling is dynamic viewport space).
- `dock_state_t` (`state.h`) - runtime mutable, session-persisted per-dock
  state: `width` (0 = collapsed) and `memoryX` (double-click restore size).
- `dock_theme_t` (`theme.h`) - shared visual style: background, grip/gripHover/
  gripActive color pairs, the slider pairs `sliderTrack` / `sliderThumb` /
  `sliderThumbHover`, and the separator color. Embedded in `theme_t`. The slider
  gets its own pairs rather than reusing the grip's because the grip is a strip
  at the dock edge with a separator beside it, so it reads at near-background
  colours a bar inside the dock body would not. Two pairs for three drawn pieces:
  the filled portion and the thumb are one idea - where the value is - and the
  thumb pair carries both.
- `row_t` (`row.h`) - one line of dock content, projected by the host the way it
  projects workspaces into `Ui::TabBar`: `{id, label, value, icon, depth,
  hasChildren, isExpanded, isSelected, RowKind kind, fpx_t ratio}`. Deliberately
  one type for both dock kinds - a tree row indents by `depth` and draws a
  chevron from `hasChildren` with `value` empty; a property row sits at depth 0
  with a non-empty right-aligned `value`; a group is a property row with
  children, so groups collapse for free. `id` is the host's own identifier,
  echoed back untouched in the row intents and opaque to the framework, and
  `INVALID_ID` marks a row that renders but never reports. `label` and `value`
  are LITERAL display text, not locale keys - the host resolves any lookup before
  projecting, exactly as `tab_t::label` does.
- `RowKind` enum (`rowkind.h`) - `Text` (every row that is read rather than
  manipulated: tree nodes, key/value properties, group headers) or `Slider`. An
  explicit kind rather than inferring "slider" from a sentinel value, because a
  slider legitimately sits at any ratio including zero, so no value is free to
  mean "not a slider". `ratio` is 0..1 and ignored unless `kind` is `Slider`; the
  framework knows only the fraction, and a host that wants the mapped number
  drawn alongside puts it in `value`.

## session.json v2

The persisted state format `serializeSession` writes and `deserializeSession`
reads (v2 only - no legacy reads). Section and key spellings are single-sourced
in `key/section.h` (`SectionKey`) and `key/session.h` (`SessionKey`).

Root scalars (file metadata, never restored):

- `version` - app version (CMake `PROJECT_VERSION`).
- `lastUpdate` - epoch milliseconds of the save.

Sections:

- `mainWindow` - `{x, y, width, height}`; frame origin + client size in physical
  px. Written only once geometry is committed (zeros would mask layout defaults).
- `view` - `{theme, themeMode}`.
- `paths` - `{lastOpenDir}` (omitted when empty).
- `files` - `{active, open[]}`; file paths in tab order, not tabs/workspaces.
- `docks` - array of `{name, width, memoryX}`; only docks that exist in the
  current layout and carry committed state are written (orphans dropped).

A host extends the file with its own domain keys via `registerPersisted`; those
keys use free-form spellings since the fw cannot enumerate them.

## Usage

```cpp
#include "ui/res/resmanager.h"

Ui::Res::ResManager res("./res");
res.loadAll();

// Read parsed values through the accessors (never copy them locally).
const Ui::Res::Type::layout_t & layout = res.layout();
const Ui::Res::Type::theme_t &  theme  = res.theme();
const Ui::Color                 barBg  = theme.topMenu.bg;
const Ui::fpx_t                 winW   = layout.windowWidth;

// A host extends session.json with its own persisted key, then wires the
// save hook. The getter returns the current value as a string; the setter
// applies a restored string back to host state.
res.registerPersisted(Ui::Res::Key::SectionKey::View,
                      "gridVisible",
                      [&host] { return host.gridVisible() ? "1" : "0"; },
                      [&host](const std::string & v) { host.setGridVisible(v == "1"); });

// ResManager owns the file too, so a save is one call. Post it off-thread if the
// host has a hook that fires often (a dock drag): build the blob on the UI thread,
// write on the worker.
res.setOnPersistChange([&res] { (void)res.saveSession(); });

// On startup, before Shell::initialize() (loadAll builds from the restored theme,
// and window creation reads the restored geometry). Tolerant of missing/garbage.
(void)res.loadSession(res.sessionPath());
```

## See also

- [shell-actions.md](shell-actions.md)
- [vocabulary.md](vocabulary.md)
- [README.md](README.md)
