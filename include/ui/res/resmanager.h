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
#include "ui/res/locale/localemanager.h"
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

#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Ui::Res {

class ResManager final {
    // One persisted user setting exposed to the host. Each setting registers a
    // (sessionKey, get, set) triple at construction; the host iterates the
    // registry (persistedSettings()) to store/restore instead of naming every
    // field. Both menu-driven settings (themeName, displayMode, ambience) and
    // programmatic ones (themeMode, lastOpenDir) flow through the same path.
    struct alignas(128) persisted_setting_t final {
        std::string                              sessionKey;          // camelCase key in session.json
        std::function<std::string()>             get;                 // current value -> string
        std::function<void(const std::string &)> set;                 // string -> apply to typed field
        bool                                     skipIfEmpty = false; // omit from save when value is empty
    };

    Store::LayoutStore             m_layoutStore; // res/css/layout.json: layout_t + popup_t (logical sub-store)
    Store::ThemeStore              m_themeStore;  // res/css/theme/*: theme_t + name/mode + preview (sub-store)
    Ui::Res::Type::input_t         m_input;
    Ui::Res::Locale::LocaleManager m_localeManager;
    id_t                           m_activeMenuId = Ui::INVALID_ID;
    std::string                    m_statusText;
    Store::IconStore               m_iconStore;     // res/icon-defaults.json (logical sub-store)
    Store::DialogStore             m_dialogStore;   // res/dialog.json (logical sub-store)
    Store::ShortcutStore           m_shortcutStore; // res/shortcut.json (logical sub-store)
    Ui::TabBar                     m_tabBar;        // chrome tab view; a host projects its tabs into it
    Ui::Res::ResPath               m_resPath;
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
    std::string              m_sessionDir  = ".pureglui";    // from app.json; read by host to locate session.json
    std::string              m_sessionFile = "session.json"; // from app.json; host owns the actual file I/O
    // Native open-dialog config (app.json "openFile"). Domain-blind: the app
    // supplies title + file-type filters; empty filters => any file.
    std::string                        m_openFileTitle;
    std::vector<Ui::Io::file_filter_t> m_openFileFilters;
    Ui::Res::Type::Changed             m_changed = Ui::Res::Type::Changed::None;
    std::vector<persisted_setting_t>   m_persistedSettings;
    // Host hook invoked whenever a persisted setting changes. The fw owns no
    // session concept; a host wires this to its own session save.
    std::function<void()> m_onPersistChange = [] {};

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
        // Load app.json for identity (title) + session storage location. The
        // session location is exposed to the host (sessionDir/sessionFile); the
        // host owns the actual session.json read/write.
        try {
            std::ifstream appJson(m_resPath.appFile());
            if (appJson.is_open()) {
                nlohmann::json app;
                appJson >> app;
                m_title       = app.value("title", m_title);
                m_sessionDir  = app.value("sessionDir", m_sessionDir);
                m_sessionFile = app.value("sessionFile", m_sessionFile);

                // Native open-dialog config (optional). Absent/empty filters =>
                // the dialog offers any file.
                if (app.contains("openFile")) {
                    const auto & openFile = app["openFile"];
                    m_openFileTitle       = openFile.value("title", m_openFileTitle);
                    for (const auto & filter : openFile.value("filters", nlohmann::json::array())) {
                        m_openFileFilters.push_back(
                        { filter.value("name", std::string {}), filter.value("spec", std::string {}) });
                    }
                }

                std::cout << "[ResManager] Loaded app.json: title=" << m_title << " sessionDir=" << m_sessionDir
                          << std::endl;
            }
        } catch (const std::exception & e) {
            std::cerr << "[ResManager] Failed to load app.json, using defaults: " << e.what() << std::endl;
        }

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
        m_menuStore.buildActionMap();
    }

    const Ui::Res::Type::layout_t &        layout() const { return m_layoutStore.layout(); }
    const Ui::Res::Type::theme_t &         theme() const { return m_themeStore.theme(); }
    const Ui::Res::Type::popup_t &         popup() const { return m_layoutStore.popup(); }
    const Ui::Res::Type::input_t &         input() const { return m_input; }
    const Ui::Res::Locale::LocaleManager & localeManager() const { return m_localeManager; }
    const std::string &                    title() const { return m_title; }
    const std::string &                    openFileTitle() const { return m_openFileTitle; }

    const std::vector<Ui::Io::file_filter_t> & openFileFilters() const { return m_openFileFilters; }
    const std::string &                        tabCloseIcon() const { return m_layoutStore.layout().tabCloseIcon; }
    const std::string &                        tabArrowLeft() const { return m_layoutStore.layout().tabArrowIconLeft; }
    const std::string & tabArrowRight() const { return m_layoutStore.layout().tabArrowIconRight; }
    const std::unordered_map<std::string, std::string> & shortcuts() const { return m_shortcutStore.shortcuts(); }

    // Resolved icon default for a role from icon-defaults.json. Returns an empty
    // icon when the role is unknown - call sites should treat that as "no marker".
    [[nodiscard]] Ui::Res::Type::icon_default_t iconDefault(const std::string & role) const
    {
        return m_iconStore.iconDefault(role);
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
    void disableUnhandledMenuItems(const std::function<bool(const std::string &)> & isHandled)
    {
        m_menuStore.disableUnhandledMenuItems(isHandled);
    }

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
        m_onPersistChange();
    }

    void setThemeName(const std::string & name)
    {
        markChanged(m_themeStore.setThemeName(name, m_resPath));
        m_onPersistChange();
    }

    [[nodiscard]] const std::string & themeName() const { return m_themeStore.themeName(); }

    // Register a host-provided value getter for a stateful (radio) action's
    // menu highlight. The fw computes the active item as item.label == provider()
    // (see isActiveItem); the host wires actions whose value lives host-side
    // (SetDisplayMode, SetAmbience). Internal-valued actions (SwitchTheme) are
    // wired by registerPersistedSettings.
    void setActionValueProvider(const std::string & actionKey, std::function<std::string()> provider)
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
        m_onPersistChange();
    }

    const std::string & lastOpenDir() const { return m_lastOpenDir; }

    void setLastOpenDir(const std::string & dir)
    {
        m_lastOpenDir = dir;
        m_onPersistChange();
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
        m_onPersistChange();
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
        m_onPersistChange();
    }

    void registerPersisted(std::string                              key,
                           std::function<std::string()>             getter,
                           std::function<void(const std::string &)> setter,
                           bool                                     skipIfEmpty = false)
    {
        m_persistedSettings.push_back({ std::move(key), std::move(getter), std::move(setter), skipIfEmpty });
    }

    void registerPersistedSettings()
    {
        registerPersisted(
        "theme",
        [this] { return m_themeStore.themeName(); },
        [this](const std::string & v) { m_themeStore.setThemeNameValue(v); });
        registerPersisted(
        "themeMode",
        [this] { return m_themeStore.themeMode(); },
        [this](const std::string & v) { m_themeStore.setThemeModeValue(v); });
        // Host domain settings live outside this fw registry; a host persists its
        // own state directly through its session.
        registerPersisted(
        "lastOpenDir",
        [this] { return m_lastOpenDir; },
        [this](const std::string & v) { m_lastOpenDir = Common::Sanitize::path(v, "lastOpenDir"); },
        /*skipIfEmpty=*/true);
        // Window geometry. Stored as int-via-string so it reuses the existing
        // string-only persisted-setting registry. skipIfEmpty drops the keys
        // from a fresh session.json until something actually committed them
        // (otherwise width/height of 0 would mask the layout default). The
        // getter/setter pairs reference the member directly through `this`
        // so the captured reference outlives the lambda.
        registerPersisted(
        "windowX",
        [this] { return m_sessionWindowWidth > 0 ? std::to_string(m_sessionWindowX) : std::string {}; },
        [this](const std::string & v) {
            try {
                m_sessionWindowX = std::stoi(v);
            } catch (...) {
                m_sessionWindowX = 0;
            }
        },
        /*skipIfEmpty=*/true);
        registerPersisted(
        "windowY",
        [this] { return m_sessionWindowHeight > 0 ? std::to_string(m_sessionWindowY) : std::string {}; },
        [this](const std::string & v) {
            try {
                m_sessionWindowY = std::stoi(v);
            } catch (...) {
                m_sessionWindowY = 0;
            }
        },
        /*skipIfEmpty=*/true);
        registerPersisted(
        "windowWidth",
        [this] { return m_sessionWindowWidth > 0 ? std::to_string(m_sessionWindowWidth) : std::string {}; },
        [this](const std::string & v) {
            try {
                m_sessionWindowWidth = std::stoi(v);
            } catch (...) {
                m_sessionWindowWidth = 0;
            }
        },
        /*skipIfEmpty=*/true);
        registerPersisted(
        "windowHeight",
        [this] { return m_sessionWindowHeight > 0 ? std::to_string(m_sessionWindowHeight) : std::string {}; },
        [this](const std::string & v) {
            try {
                m_sessionWindowHeight = std::stoi(v);
            } catch (...) {
                m_sessionWindowHeight = 0;
            }
        },
        /*skipIfEmpty=*/true);

        // Stateful (radio) actions: current value drives the highlight - a menu
        // item is active when item.label == this value. SwitchTheme's value is
        // fw-internal (themeName); SetDisplayMode/SetAmbience live host-side and
        // are wired by the host via setActionValueProvider.
        m_menuStore.setActionValueProvider("SwitchTheme", [this] { return m_themeStore.themeName(); });
    }

    // Host seam for persistence. The fw owns no session FILE - a host reads/writes
    // the blob wherever it likes (sessionDir()/sessionFile() are the suggested
    // location, from app.json) and wires setOnPersistChange to its own save. The
    // fw owns the FORMAT: serializeSession()/deserializeSession() encode/decode
    // the persisted state, so JSON never leaks into the host.
    void setOnPersistChange(std::function<void()> onPersistChange) { m_onPersistChange = std::move(onPersistChange); }

    [[nodiscard]] const std::string & sessionDir() const { return m_sessionDir; }
    [[nodiscard]] const std::string & sessionFile() const { return m_sessionFile; }

    // Encode all persisted state (window geometry + the persisted-setting registry
    // + dock state) to an opaque session blob the host stores verbatim.
    [[nodiscard]] std::string serializeSession() const
    {
        nlohmann::json j;
        j["window"] = { { "x", m_sessionWindowX },
                        { "y", m_sessionWindowY },
                        { "width", m_sessionWindowWidth },
                        { "height", m_sessionWindowHeight } };

        nlohmann::json settings;
        for (const auto & setting : m_persistedSettings) {
            settings[setting.sessionKey] = setting.get();
        }
        j["settings"] = std::move(settings);

        nlohmann::json docks;
        m_dockStore.writeDockJson(docks);
        j["docks"] = std::move(docks);

        return j.dump(2);
    }

    // Restore persisted state from a blob produced by serializeSession. Tolerant
    // of missing/garbage data. Geometry lands in the session fields (the window
    // picks it up at creation); theme name/mode are restored and the file reloaded.
    void deserializeSession(const std::string & data)
    {
        const nlohmann::json j = nlohmann::json::parse(data, nullptr, /*allow_exceptions=*/false);
        if (!j.is_object()) {
            return;
        }
        if (j.contains("window") && j["window"].is_object()) {
            const auto & w = j["window"];
            setSessionWindowGeometry(w.value("x", 0), w.value("y", 0), w.value("width", 0), w.value("height", 0));
        }
        if (j.contains("settings") && j["settings"].is_object()) {
            const auto & settings = j["settings"];
            for (const auto & setting : m_persistedSettings) {
                if (auto it = settings.find(setting.sessionKey); it != settings.end() && it->is_string()) {
                    setting.set(it->get<std::string>());
                }
            }
            // The persisted setters store the raw theme name/mode; reload the
            // theme file now for the restored pair.
            markChanged(m_themeStore.loadCurrent(m_resPath));
        }
        if (j.contains("docks")) {
            m_dockStore.readDockJson(j["docks"]);
        }
    }

    // Load resource methods
    void loadInput(const std::string & file)
    {
        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return;
        }

        if (j.contains("scroll") && j["scroll"].is_object()) {
            const auto & scroll         = j["scroll"];
            m_input.scrollNatural       = scroll.value("natural", m_input.scrollNatural);
            m_input.scrollSpeed         = scroll.value("speed", m_input.scrollSpeed);
            m_input.scrollSmooth        = scroll.value("smooth", m_input.scrollSmooth);
            m_input.scrollSnapThreshold = scroll.value("snapThreshold", m_input.scrollSnapThreshold);
        }

        if (j.contains("keyAnimation") && j["keyAnimation"].is_object()) {
            const auto & keyAnimation = j["keyAnimation"];
            m_input.keyAnimationDelay = keyAnimation.value("delay", m_input.keyAnimationDelay);
        }
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
            nlohmann::json j;
            if (!Common::loadJson(entry.path().string(), j)) {
                continue;
            }
            Ui::Res::Dock::dock_config_t cfg;
            cfg.name         = Common::Sanitize::string(j.value("name", std::string {}),
                                                "dock.name",
                                                Store::DockStore::MAX_DOCK_NAME_LENGTH);
            cfg.anchor       = Ui::Res::Dock::dockAnchorFromName(j.value("anchor", std::string { "left" }));
            cfg.order        = j.value("order", 1);
            cfg.defaultWidth = Ui::Convert::str2fpx(j.value("default-width", std::string {}));
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
