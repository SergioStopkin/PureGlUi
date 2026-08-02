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
#include "common/fs.h"
#include "common/json.h"
#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/convert.h"
#include "ui/io/filefilter.h"
#include "ui/res/dock/state.h"
#include "ui/res/key/app.h"
#include "ui/res/key/dock.h"
#include "ui/res/key/iconrole.h"
#include "ui/res/key/section.h"
#include "ui/res/key/session.h"
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/store/dialogstore.h"
#include "ui/res/store/dockstore.h"
#include "ui/res/store/iconstore.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/store/menustore.h"
#include "ui/res/store/shortcutstore.h"
#include "ui/res/store/themestore.h"
#include "ui/res/type/button.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/icondefault.h"
#include "ui/res/type/input.h"
#include "ui/res/type/layout.h"
#include "ui/res/type/menu.h"
#include "ui/res/type/popup.h"
#include "ui/res/type/theme.h"
#include "ui/tabbar.h"
#include "ui/type.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Res {

class ResManager final {
    // One persisted user setting exposed to the host. Each setting registers a
    // (sessionKey, get, set) triple at construction; the host iterates the
    // registry (persistedSettings()) to store/restore instead of naming every
    // field. Both menu-driven settings (themeName, displayMode, ambience) and
    // programmatic ones (themeMode, lastOpenDir) flow through the same path.
    struct alignas(128) persisted_setting_t final {
        std::string       section;             // resolved session.json section name (fw or host enum)
        std::string       sessionKey;          // camelCase key inside the section (empty when isJsonValue)
        Ui::provider_fn_t get;                 // current value -> string
        Ui::action_fn_t   set;                 // string -> apply to typed field
        bool              skipIfEmpty = false; // omit from save when value is empty
        bool              isJsonValue = false; // value is JSON text and owns the whole section
    };

    Store::LayoutStore     m_layoutStore; // res/layout.json: layout_t + popup_t (logical sub-store)
    Store::ThemeStore      m_themeStore;  // res/submenu/theme/*: theme_t + name/mode + preview (sub-store)
    Ui::Res::Type::input_t m_input;
    Ui::Res::LocaleManager m_localeManager;
    id_t                   m_activeMenuId = Ui::INVALID_ID;
    std::string            m_statusText;
    Store::IconStore       m_iconStore;     // res/icon-defaults.json (logical sub-store)
    Store::DialogStore     m_dialogStore;   // res/dialog.json (logical sub-store)
    Store::ShortcutStore   m_shortcutStore; // res/shortcut.json (logical sub-store)
    Ui::TabBar             m_tabBar;        // chrome tab view; a host projects its tabs into it
    Ui::Res::ResPath       m_resPath;
    // menus/buttons/action-map machinery; composes locale + icon/theme/layout
    // stores (declared above) + resPath, so it is declared after them.
    Store::MenuStore m_menuStore { m_localeManager, m_iconStore, m_themeStore, m_layoutStore, m_resPath };
    Store::DockStore m_dockStore { m_layoutStore }; // session per-dock state (reads layout dock config)
    std::string      m_lastOpenDir;
    // Persisted main-window geometry. width/height <= 0 means "not set yet" -
    // WindowManager falls back to layout.windowWidth/Height on first launch.
    // x/y default to 0 (top-left), only meaningful once width/height are set.
    int m_sessionWindowX      = 0;
    int m_sessionWindowY      = 0;
    int m_sessionWindowWidth  = 0;
    int m_sessionWindowHeight = 0;
    // Persisted open-files list (paths, in tab order) plus the active file path.
    // A host restores these on startup so the user sees the same tabs in the same
    // order as when they quit. Active is keyed by path (not id) because tab ids
    // are not stable across runs.
    std::vector<std::string> m_sessionOpenFiles;
    std::string              m_sessionActiveFile;
    std::string              m_title       = "PureGlUi";
    std::string              m_sessionDir  = ".pureglui";    // from app.json; loadSession/saveSession use it
    std::string              m_sessionFile = "session.json"; // from app.json; joined by sessionPath()
    // Native open-dialog config (app.json "openFile"). Domain-blind: the app
    // supplies title + file-type filters; empty filters => any file.
    std::string                        m_openFileTitle;
    std::vector<Ui::Io::file_filter_t> m_openFileFilters;
    Ui::Res::Type::Changed             m_changed = Ui::Res::Type::Changed::None;
    std::vector<persisted_setting_t>   m_persistedSettings;
    // Host hook invoked whenever a persisted setting changes. The fw owns no
    // session concept; a host wires this to its own session save.
    Ui::task_fn_t m_onPersistChange = [] {};
    bool          m_isRestoring     = false; // true while deserializeSession applies values

    // Accumulate a Changed flag into the pending reload mask. Sub-store loads
    // and runtime setters funnel their Changed return through here.
    void markChanged(Ui::Res::Type::Changed flag)
    {
        m_changed = static_cast<Ui::Res::Type::Changed>(Common::Bit::Or(m_changed, flag));
    }

public:
    explicit ResManager(std::string resDir = "./res")
        : m_resPath(std::move(resDir))
    {
        // Load app.json for identity (title) + session storage location, which
        // loadSession/saveSession then use. Every fallback is the default already
        // in hand, so a missing or malformed file keeps all of them - and no read
        // below can throw, which is why there is no try/catch any more.
        using Ui::Res::Key::AppKey;
        const auto app = Common::loadJson(m_resPath.appFile());
        m_title = Common::Sanitize::string(Common::Json::string(app, appKeyName(AppKey::Title), m_title), "app.title");
        m_sessionDir = Common::Sanitize::filePath(
        Common::Json::string(app, appKeyName(AppKey::SessionDir), m_sessionDir),
        "app.sessionDir");
        m_sessionFile = Common::Sanitize::filePath(
        Common::Json::string(app, appKeyName(AppKey::SessionFile), m_sessionFile),
        "app.sessionFile");

        // Native open-dialog config (optional). Absent/empty filters => the
        // dialog offers any file.
        const auto & openFile = Common::Json::object(app, appKeyName(AppKey::OpenFile));
        m_openFileTitle       = Common::Sanitize::string(
        Common::Json::string(openFile, appKeyName(AppKey::Title), m_openFileTitle),
        "app.openFile.title");
        for (const auto & filter : Common::Json::array(openFile, appKeyName(AppKey::Filters))) {
            m_openFileFilters.push_back(
            { Common::Sanitize::string(Common::Json::string(filter, appKeyName(AppKey::Name)), "app.filter.name"),
              Common::Sanitize::string(Common::Json::string(filter, appKeyName(AppKey::Spec)), "app.filter.spec") });
        }

        std::cout << "[ResManager] Loaded app.json: title=" << m_title << " sessionDir=" << m_sessionDir << std::endl;

        // Build the persisted-setting registry (raw getters/setters over the
        // owned fields). The host drives load/save through persistedSettings().
        registerPersistedSettings();
    }
    void loadAll()
    {
        m_changed = Ui::Res::Type::Changed::None;
        m_localeManager.load("en", m_resPath);
        markChanged(Ui::Res::Type::Changed::Locale);
        markChanged(m_themeStore.loadCurrent(m_resPath));
        markChanged(m_layoutStore.load(m_resPath.layoutFile()));
        loadInput(m_resPath.inputFile());
        m_dialogStore.load(m_resPath.dialogFile());
        markChanged(m_shortcutStore.load(m_resPath.shortcutFile()));
        markChanged(m_iconStore.load(m_resPath.iconDefaultsFile()));
        markChanged(m_menuStore.loadButtons(m_resPath.buttonDir()));
        // loadMenus enumerates names for any menu item declaring
        // `"submenus": {"auto": "<key>"}` (e.g. theme). Any matching domain config
        // load is the host's job, done separately - the fw only builds menu items.
        markChanged(m_menuStore.loadMenus(m_resPath.menuDir()));
        loadDocks(m_resPath.dockDir());
        m_menuStore.buildIndexes();
    }

    const Ui::Res::Type::layout_t & layout() const { return m_layoutStore.layout(); }
    const Ui::Res::Type::theme_t &  theme() const { return m_themeStore.theme(); }
    const Ui::Res::Type::popup_t &  popup() const { return m_layoutStore.popup(); }
    const Ui::Res::Type::input_t &  input() const { return m_input; }
    const Ui::Res::LocaleManager &  localeManager() const { return m_localeManager; }
    const std::string &             title() const { return m_title; }
    const std::string &             openFileTitle() const { return m_openFileTitle; }

    const std::vector<Ui::Io::file_filter_t> & openFileFilters() const { return m_openFileFilters; }
    const std::string &                        tabCloseIcon() const { return m_layoutStore.layout().tabCloseIcon; }
    const std::string &                        tabArrowLeft() const { return m_layoutStore.layout().tabArrowIconLeft; }
    const std::string & tabArrowRight() const { return m_layoutStore.layout().tabArrowIconRight; }
    const std::unordered_map<std::string, std::string> & shortcuts() const { return m_shortcutStore.shortcuts(); }

    // Resolved icon default for a role from icon-defaults.json. Returns an empty
    // icon when the role is unknown - call sites should treat that as "no marker".
    // The string overload serves data-driven roles (menu JSON etc.); fw code uses
    // the IconRoleKey overload so its role spellings stay single-sourced.
    [[nodiscard]] Ui::Res::Type::icon_default_t iconDefault(const std::string & role) const
    {
        return m_iconStore.iconDefault(role);
    }

    [[nodiscard]] Ui::Res::Type::icon_default_t iconDefault(Ui::Res::Key::IconRoleKey role) const
    {
        return m_iconStore.iconDefault(Ui::Res::Key::iconRoleKeyName(role));
    }

    // True when this item is the active choice of its stateful action - i.e.
    // its label (the value) equals the action's current value. Plain items and
    // items whose action carries no state are never active. Computed live; no
    // cache, no group, no hardcoded names.
    [[nodiscard]] bool isActiveItem(const Ui::Res::Type::menu_t & item) const { return m_menuStore.isActiveItem(item); }

    // Theme preview colors for a given theme key: {dark, light} where each
    // variant is {cl-main, bg-main}. Returns zero-init pair if absent.
    [[nodiscard]] std::pair<Ui::Res::Type::color_pair_t, Ui::Res::Type::color_pair_t>
    themePreviewColors(const std::string & key) const
    {
        return m_themeStore.themePreviewColors(key);
    }

    [[nodiscard]] const Ui::Res::Type::dialog_type_config_t & dialogTypeConfig(Ui::Res::Type::DialogType type) const
    {
        return m_dialogStore.dialogTypeConfig(type);
    }

    [[nodiscard]] std::string actionKeyFor(id_t elementId) const { return m_menuStore.actionKeyFor(elementId); }

    [[nodiscard]] Ui::Res::Type::menu_t findMenuItem(id_t elementId) const
    {
        return m_menuStore.findMenuItem(elementId);
    }

    [[nodiscard]] Ui::Res::Type::menu_t findMenuItemByKey(const Ui::key_t & key) const
    {
        return m_menuStore.findMenuItemByKey(key);
    }

    const std::vector<Ui::Res::Type::button_t> & buttons() const { return m_menuStore.buttons(); }
    const std::vector<Ui::Res::Type::menu_t> &   menus() const { return m_menuStore.menus(); }

    // Mark every menu item bound to `action` as enabled / disabled.
    // Recurses into submenus so radio-group children
    // (e.g. ambience choices nested under a parent host) are also caught.
    // Runtime equivalent of the JSON "enabled": false field;
    // used to gate features against hardware capability
    // (e.g. disable IBL-dependent ambience choices on software GL).
    void setActionEnabled(const std::string & actionKey, bool enabled)
    {
        m_menuStore.setActionEnabled(actionKey, enabled);
    }

    // Disable / enable the menu item bound to (action, label). Used for
    // parameterised actions like the ambience setter where many menu items
    // share the same actionKey and the label identifies which preset is
    // being gated. setActionEnabled would disable all sharers; this is
    // the per-label variant.
    void setMenuItemEnabled(const std::string & actionKey, const std::string & label, bool enabled)
    {
        m_menuStore.setMenuItemEnabled(actionKey, label, enabled);
    }

    // Disable menu nodes that can do nothing: a leaf whose actionKey has no
    // handler, or a parent (submenu / top-menu) whose every child is disabled
    // (`isHandled` reports handler presence). Dialog items stay enabled.
    void disableUnhandledItems(const Ui::predicate_fn_t & isHandled) { m_menuStore.disableUnhandledItems(isHandled); }

    Ui::TabBar &                         tabBar() { return m_tabBar; }
    const Ui::TabBar &                   tabBar() const { return m_tabBar; }
    id_t                                 activeMenuId() const { return m_activeMenuId; }
    void                                 setActiveMenu(id_t id) { m_activeMenuId = id; }
    void                                 clearActiveMenu() { m_activeMenuId = Ui::INVALID_ID; }
    const std::string &                  statusText() const { return m_statusText; }
    void                                 setStatusText(const std::string & text) { m_statusText = text; }
    [[nodiscard]] Ui::Res::Type::Changed changed() const { return m_changed; }

    [[nodiscard]] const Ui::Res::ResPath & resPath() const { return m_resPath; }

    [[nodiscard]] bool isThemeDark() const { return m_themeStore.isThemeDark(); }

    [[nodiscard]] std::string themeIcon() const { return m_themeStore.themeIcon(); }

    void switchThemeMode()
    {
        markChanged(m_themeStore.switchThemeMode(m_resPath));
        notifyPersistChange();
    }

    void setThemeName(const std::string & name)
    {
        markChanged(m_themeStore.setThemeName(name, m_resPath));
        notifyPersistChange();
    }

    [[nodiscard]] const std::string & themeName() const { return m_themeStore.themeName(); }

    // Register a host-provided value getter for a stateful (radio) action's
    // menu highlight. The fw computes the active item as item.label == provider()
    // (see isActiveItem); the host wires actions whose value lives host-side
    // (SetDisplayMode, SetAmbience). Internal-valued actions (SwitchTheme) are
    // wired by registerPersistedSettings.
    void setActionValueProvider(const std::string & actionKey, Ui::provider_fn_t provider)
    {
        m_menuStore.setActionValueProvider(actionKey, std::move(provider));
    }

    // Per-dock session state (collapsed if absent). Delegates to DockStore.
    [[nodiscard]] Ui::Res::Dock::dock_state_t dockState(const std::string & name) const
    {
        return m_dockStore.dockState(name);
    }

    // Commit a dock's state, then fire the persist hook. Marked const (mutable
    // storage in DockStore) so a subsystem holding `const ResManager&` (e.g.
    // WindowManager) can write dock prefs. NOT thread-safe; UI-thread callers only.
    void setDockState(const std::string & name, const Ui::Res::Dock::dock_state_t & state) const
    {
        m_dockStore.setDockState(name, state);
        notifyPersistChange();
    }

    const std::string & lastOpenDir() const { return m_lastOpenDir; }

    void setLastOpenDir(const std::string & dir)
    {
        m_lastOpenDir = dir;
        notifyPersistChange();
    }

    [[nodiscard]] int sessionWindowX() const { return m_sessionWindowX; }
    [[nodiscard]] int sessionWindowY() const { return m_sessionWindowY; }
    [[nodiscard]] int sessionWindowWidth() const { return m_sessionWindowWidth; }
    [[nodiscard]] int sessionWindowHeight() const { return m_sessionWindowHeight; }

    [[nodiscard]] const std::vector<std::string> & sessionOpenFiles() const { return m_sessionOpenFiles; }
    [[nodiscard]] const std::string &              sessionActiveFile() const { return m_sessionActiveFile; }

    // Commit the open-files snapshot. Diff-checked so a no-op snapshot
    // (same paths in same order, same active) doesn't post a disk write.
    void setSessionOpenFiles(std::vector<std::string> files, std::string activeFile)
    {
        if (m_sessionOpenFiles == files && m_sessionActiveFile == activeFile) {
            return;
        }
        m_sessionOpenFiles  = std::move(files);
        m_sessionActiveFile = std::move(activeFile);
        notifyPersistChange();
    }

    // Commit main-window geometry. Diff-checked so a stream of identical
    // ConfigureNotify events (X11 emits one per pixel during a drag) doesn't
    // post redundant disk writes. The actual write is off-thread inside the
    // host's session save, so even unique values are cheap on the UI.
    void setSessionWindowGeometry(int x, int y, int width, int height)
    {
        if (m_sessionWindowX == x && m_sessionWindowY == y && m_sessionWindowWidth == width
            && m_sessionWindowHeight == height) {
            return;
        }
        m_sessionWindowX      = x;
        m_sessionWindowY      = y;
        m_sessionWindowWidth  = width;
        m_sessionWindowHeight = height;
        notifyPersistChange();
    }

    // A section is either owned by one JSON provider or carries plain keys,
    // never both: serializeSession assigns the whole node for an owner and
    // string-subscripts it for a key, so mixing them either throws (subscripting
    // an array) or silently drops the keys, depending purely on registration
    // order. Both symptoms surface at SAVE time, far from the registration that
    // caused them, so the conflict is refused here instead.
    [[nodiscard]] bool isSectionOwned(const std::string & section) const
    {
        return std::any_of(
        m_persistedSettings.begin(),
        m_persistedSettings.end(),
        [&section](const persisted_setting_t & setting) { return setting.section == section && setting.isJsonValue; });
    }

    [[nodiscard]] bool hasSectionKeys(const std::string & section) const
    {
        return std::any_of(
        m_persistedSettings.begin(),
        m_persistedSettings.end(),
        [&section](const persisted_setting_t & setting) { return setting.section == section && !setting.isJsonValue; });
    }

    // Section first, then key: arguments read outer -> inner, like the JSON
    // they produce (j[sectionKey][key]). Keys go in FRAMEWORK sections - the fw
    // owns those names and a host adds its domain keys to them.
    void registerPersisted(Ui::Res::Key::SectionKey section,
                           std::string              key,
                           Ui::provider_fn_t        getter,
                           Ui::action_fn_t          setter,
                           bool                     skipIfEmpty = false)
    {
        m_persistedSettings.push_back(
        { Ui::Res::Key::sectionKeyName(section), std::move(key), std::move(getter), std::move(setter), skipIfEmpty });
    }

    // Structured variant: the getter returns JSON TEXT, which lands in the file
    // as real JSON rather than an escaped string, and the setter gets it back the
    // same way. One provider owns a whole section (arrays included - the
    // section/key registry above cannot express a root-level array).
    //
    // The section is the HOST's: it brings its own enum and its own
    // sectionKeyName() overload, found here by ADL, so the fw stores a resolved
    // name and never learns a host concept. SectionKey is excluded by the
    // constraint, so claiming a framework section fails to COMPILE; the checks
    // below only catch a host mapper that returns a framework name anyway.
    template <typename SectionEnum>
        requires std::is_enum_v<SectionEnum> && (!std::is_same_v<SectionEnum, Ui::Res::Key::SectionKey>)
    void registerPersistedJson(SectionEnum hostSection, Ui::provider_fn_t getter, Ui::action_fn_t setter)
    {
        std::string section = sectionKeyName(hostSection);
        if (section.empty() || Ui::Res::Key::isFrameworkSection(section) || isSectionOwned(section)
            || hasSectionKeys(section)) {
            std::cerr << "[ResManager] Section '" << section
                      << "' is unusable, framework-owned, or already registered; ignoring JSON provider" << std::endl;
            return;
        }
        m_persistedSettings.push_back({ std::move(section),
                                        /*sessionKey=*/ {},
                                        std::move(getter),
                                        std::move(setter),
                                        /*skipIfEmpty=*/true,
                                        /*isJsonValue=*/true });
    }

    void registerPersistedSettings()
    {
        registerPersisted(
        Ui::Res::Key::SectionKey::View,
        Ui::Res::Key::sessionKeyName(Ui::Res::Key::SessionKey::Theme),
        [this] { return m_themeStore.themeName(); },
        [this](const std::string & v) { m_themeStore.setThemeNameValue(v); });
        registerPersisted(
        Ui::Res::Key::SectionKey::View,
        Ui::Res::Key::sessionKeyName(Ui::Res::Key::SessionKey::ThemeMode),
        [this] { return m_themeStore.themeMode(); },
        [this](const std::string & v) { m_themeStore.setThemeModeValue(v); });
        // Host domain settings live outside this fw registry; a host persists its
        // own state directly through its session (free-form string keys).
        registerPersisted(
        Ui::Res::Key::SectionKey::Paths,
        Ui::Res::Key::sessionKeyName(Ui::Res::Key::SessionKey::LastOpenDir),
        [this] { return m_lastOpenDir; },
        [this](const std::string & v) { m_lastOpenDir = Common::Sanitize::path(v, "lastOpenDir"); },
        /*skipIfEmpty=*/true);
        // Window geometry is NOT in this string registry: it lives in the typed
        // "mainWindow" block serializeSession writes directly - keeping it in
        // both places duplicated the same four values in every session.json.

        // Stateful (radio) actions: current value drives the highlight - a menu
        // item is active when item.label == this value. SwitchTheme's value is
        // fw-internal (themeName); SetDisplayMode/SetAmbience live host-side and
        // are wired by the host via setActionValueProvider.
        m_menuStore.setActionValueProvider("SwitchTheme", [this] { return m_themeStore.themeName(); });
    }

    // Host seam for persistence: wire this to whatever should happen when a
    // persisted value changes (typically saveSession(), possibly off-thread). The
    // fw owns the FORMAT (serializeSession/deserializeSession) so JSON never leaks
    // into the host, AND the default LOCATION (sessionDir/sessionFile come from
    // app.json), so loadSession/saveSession below do the file I/O too - otherwise
    // every host re-writes the same few lines of stream code. A host that wants a
    // different sink ignores them and uses the serialize/deserialize pair.
    void setOnPersistChange(Ui::task_fn_t onPersistChange) { m_onPersistChange = std::move(onPersistChange); }

    // True while deserializeSession is applying loaded values. Restoring a session
    // walks the same setters a user edit does, so without this every restore would
    // trigger a save of what was just read. A host whose own state objects fire
    // their own persist hooks (reached through the registry setters) checks this in
    // its save path for the same reason.
    [[nodiscard]] bool isRestoringSession() const { return m_isRestoring; }

    [[nodiscard]] const std::string & sessionDir() const { return m_sessionDir; }
    [[nodiscard]] const std::string & sessionFile() const { return m_sessionFile; }

    // Fire the host's persist hook unless we are mid-restore.
    void notifyPersistChange() const
    {
        if (!m_isRestoring) {
            m_onPersistChange();
        }
    }

    // <sessionDir>/<sessionFile>, both from app.json.
    [[nodiscard]] std::string sessionPath() const
    {
        return (std::filesystem::path(m_sessionDir) / m_sessionFile).string();
    }

    // Read the session blob back - the counterpart of writeSession. Takes the path
    // explicitly (sessionPath() for the default location) so a host can keep
    // sessions elsewhere and a test never touches the real one. Call BEFORE
    // Shell::initialize(): loadAll() then builds from the saved theme, and window
    // creation reads the saved geometry. Missing file = fresh start, not an error,
    // so this returns false only when there was something to read and it failed.
    bool loadSession(const std::string & path)
    {
        if (!std::filesystem::exists(path)) {
            std::cout << "[ResManager] No existing session file, starting fresh" << std::endl;
            return true;
        }
        // const: nothing here mutates the stream object itself - rdbuf() is a
        // const member and the read happens through the buffer it returns.
        const std::ifstream file(path);
        if (!file) {
            std::cerr << "[ResManager] Failed to open session file: " << path << std::endl;
            return false;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        deserializeSession(buffer.str());
        return true;
    }

    // Write the session blob. Writes a sibling temp file and renames it over the
    // target, so a crash - or a second save racing this one - can never leave a
    // half-written session.json behind; rename is atomic within one filesystem.
    bool saveSession() const { return writeSession(sessionPath(), serializeSession()); }

    // The write half of saveSession as a free-standing step, for a host that builds
    // the blob on the UI thread and posts the disk write to a worker.
    static bool writeSession(const std::string & sessionPath, const std::string & blob)
    {
        try {
            const std::filesystem::path target(sessionPath);
            if (target.has_parent_path()) {
                std::filesystem::create_directories(target.parent_path());
            }
            std::filesystem::path temp = target;
            temp += ".tmp";
            {
                std::ofstream file(temp, std::ios::trunc);
                if (!file) {
                    std::cerr << "[ResManager] Failed to open session temp file: " << temp.string() << std::endl;
                    return false;
                }
                file << blob;
            }
            std::filesystem::rename(temp, target);
            std::cout << "[ResManager] Saved session to " << sessionPath << std::endl;
            return true;
        } catch (const std::exception & e) {
            std::cerr << "[ResManager] Failed to save session: " << e.what() << std::endl;
            return false;
        }
    }

    // Encode all persisted state (window geometry + the persisted-setting registry
    // + dock state) to an opaque session blob the host stores verbatim.
    [[nodiscard]] std::string serializeSession() const
    {
        using Ui::Res::Key::SectionKey;
        using Ui::Res::Key::SessionKey;

        nlohmann::json j;
        // Session format v2 (sectioned; see SectionKey/SessionKey). version
        // is the app version from the build (CMake PROJECT_VERSION); lastUpdate
        // is epoch milliseconds of this save.
        j[sessionKeyName(SessionKey::Version)]    = APP_VERSION;
        j[sessionKeyName(SessionKey::LastUpdate)] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                    std::chrono::system_clock::now().time_since_epoch())
                                                    .count();

        // Geometry only once committed - zeros would mask the layout defaults.
        if (m_sessionWindowWidth > 0 && m_sessionWindowHeight > 0) {
            j[sectionKeyName(SectionKey::MainWindow)] = {
                { sessionKeyName(SessionKey::X), m_sessionWindowX },
                { sessionKeyName(SessionKey::Y), m_sessionWindowY },
                { sessionKeyName(SessionKey::Width), m_sessionWindowWidth },
                { sessionKeyName(SessionKey::Height), m_sessionWindowHeight },
            };
        }

        // Key by key, not a whole-node assignment, so a registry key sharing this
        // section survives.
        if (!m_sessionOpenFiles.empty() || !m_sessionActiveFile.empty()) {
            nlohmann::json & files                    = j[sectionKeyName(SectionKey::Files)];
            files[sessionKeyName(SessionKey::Active)] = m_sessionActiveFile;
            files[sessionKeyName(SessionKey::Open)]   = m_sessionOpenFiles;
        }

        nlohmann::json docks = nlohmann::json::array();
        m_dockStore.writeDockJson(docks);
        if (!docks.empty()) {
            j[sectionKeyName(SectionKey::Docks)] = std::move(docks);
        }

        // Registry LAST, so a host key registered into a framework section adds
        // to a node that already exists instead of racing the typed block for
        // it. Host PROVIDERS cannot land here at all - a framework name is
        // refused at registration - so nothing below has to defend against it.
        for (const auto & setting : m_persistedSettings) {
            const std::string value = setting.get();
            if (setting.skipIfEmpty && value.empty()) {
                continue;
            }
            if (setting.isJsonValue) {
                // Host-authored text: a malformed value costs that one section,
                // never the whole save (parse is non-throwing).
                nlohmann::json parsed = nlohmann::json::parse(value, nullptr, /*allow_exceptions=*/false);
                if (parsed.is_discarded()) {
                    std::cerr << "[ResManager] Skipping unparseable session section: " << setting.section << std::endl;
                    continue;
                }
                j[setting.section] = std::move(parsed);
                continue;
            }
            j[setting.section][setting.sessionKey] = value;
        }

        // Paths reach this blob verbatim, and a filename is not required to be
        // valid UTF-8 (legal on Linux; Sanitize only strips control chars).
        // Default dump() throws on such bytes, and that throw would escape
        // saveSession() and cost the user every persisted setting, so replace
        // the offending bytes instead of failing the save.
        return j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
    }

    // Restore persisted state from a blob produced by serializeSession (format
    // v2 only - reads exactly what serializeSession writes). Tolerant of
    // missing/garbage data. Geometry lands in the session fields (the window
    // picks it up at creation); theme name/mode are restored and the file
    // reloaded. session.json is user-editable, so ingested paths pass Sanitize.
    void deserializeSession(const std::string & data)
    {
        // Suppress the persist hook while we apply: restoring walks the same
        // setters a user edit does. Cleared on both exits below; the parse is
        // non-throwing (allow_exceptions=false), so there is no third path.
        m_isRestoring = true;

        using Ui::Res::Key::SectionKey;
        using Ui::Res::Key::SessionKey;

        const nlohmann::json j = nlohmann::json::parse(data, nullptr, /*allow_exceptions=*/false);
        if (!j.is_object()) {
            m_isRestoring = false;
            return;
        }

        const std::string mainWindowKey = sectionKeyName(SectionKey::MainWindow);
        const auto &      mainWindow    = Common::Json::object(j, mainWindowKey);
        if (!mainWindow.empty()) {
            setSessionWindowGeometry(Common::Json::number(mainWindow, sessionKeyName(SessionKey::X), 0),
                                     Common::Json::number(mainWindow, sessionKeyName(SessionKey::Y), 0),
                                     Common::Json::number(mainWindow, sessionKeyName(SessionKey::Width), 0),
                                     Common::Json::number(mainWindow, sessionKeyName(SessionKey::Height), 0));
        }

        for (const auto & setting : m_persistedSettings) {
            // One lookup, then read through the iterator - contains() plus three
            // indexings hashed the same key four times per setting.
            //
            // Raw, not a Json:: reader: those answer "absent" and "present but
            // empty" alike, and the difference decides whether a host setter runs
            // at all. Calling one with "" for a value the session never stored
            // would clear that state instead of leaving it.
            const auto section = j.find(setting.section);
            if (section == j.end()) {
                continue;
            }
            if (setting.isJsonValue) {
                // Handed back as text, exactly as the getter produced it. A host
                // section may be an array, so this cannot go through object().
                setting.set(section->dump());
                continue;
            }
            if (!section->is_object()) {
                continue;
            }
            if (auto it = section->find(setting.sessionKey); it != section->end() && it->is_string()) {
                setting.set(it->get<std::string>());
            }
        }
        // The persisted setters store the raw theme name/mode; reload the
        // theme file now for the restored pair.
        markChanged(m_themeStore.loadCurrent(m_resPath));

        m_sessionActiveFile.clear();
        m_sessionOpenFiles.clear();
        const std::string filesKey = sectionKeyName(SectionKey::Files);
        const auto &      files    = Common::Json::object(j, filesKey);
        m_sessionActiveFile = Common::Sanitize::path(Common::Json::string(files, sessionKeyName(SessionKey::Active)),
                                                     "files.active");
        for (const auto & file : Common::Json::array(files, sessionKeyName(SessionKey::Open))) {
            if (file.is_string()) {
                m_sessionOpenFiles.emplace_back(Common::Sanitize::path(file.get<std::string>(), "files.open"));
            }
        }

        // readDockJson clears first and ignores a non-array, so an absent section
        // arrives as an empty array and means the same thing: no dock state.
        m_dockStore.readDockJson(Common::Json::array(j, sectionKeyName(SectionKey::Docks)));

        m_isRestoring = false;
    }

    // Load resource methods
    void loadInput(const std::string & file)
    {
        const auto j = Common::loadJson(file);

        const auto & scroll         = Common::Json::object(j, "scroll");
        m_input.scrollNatural       = Common::Json::boolean(scroll, "natural", m_input.scrollNatural);
        m_input.scrollSpeed         = Common::Json::number(scroll, "speed", m_input.scrollSpeed);
        m_input.scrollSmooth        = Common::Json::number(scroll, "smooth", m_input.scrollSmooth);
        m_input.scrollSnapThreshold = Common::Json::number(scroll, "snapThreshold", m_input.scrollSnapThreshold);

        const auto & keyAnimation = Common::Json::object(j, "keyAnimation");
        m_input.keyAnimationDelay = Common::Json::number(keyAnimation, "delay", m_input.keyAnimationDelay);
    }

    // Each file under res/dock/ describes one dock (anchor, order, default
    // width). Filename ordering is cosmetic - the dock's `order` field drives
    // on-screen position. Files without a valid name are skipped (the name
    // is the persistence key and the renderer-content selector).
    void loadDocks(const std::string & dir)
    {
        if (!Common::dirExists(dir)) {
            return;
        }

        std::vector<Ui::Res::Dock::dock_config_t> docks;
        for (const auto & entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() != ".json") {
                continue;
            }
            // Also the unreadable-file exit: loadJson has logged why, and a null
            // json yields no name, which is already the skip condition below.
            const auto j = Common::loadJson(entry.path().string());
            using Ui::Res::Key::DockKey;
            Ui::Res::Dock::dock_config_t cfg;
            cfg.name   = Common::Sanitize::string(Common::Json::string(j, dockKeyName(DockKey::Name)),
                                                "dock.name",
                                                Store::DockStore::MAX_DOCK_NAME_LENGTH);
            cfg.anchor = Ui::Res::Dock::dockAnchorFromName(
            Common::Json::string(j, dockKeyName(DockKey::Anchor), "left"));
            cfg.order        = Common::Json::number(j, dockKeyName(DockKey::Order), 1);
            cfg.defaultWidth = Ui::Convert::str2fpx(Common::Json::string(j, dockKeyName(DockKey::DefaultWidth)));
            if (cfg.name.empty()) {
                std::cerr << "[ResManager] dock file missing name, skipping: " << entry.path() << std::endl;
                continue;
            }
            docks.emplace_back(std::move(cfg));
        }

        // Dock definitions live in layout_t; commit them through LayoutStore.
        markChanged(m_layoutStore.setDocks(std::move(docks)));
    }
};

} // namespace Ui::Res
