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

#include "common/fs.h"
#include "common/json.h"
#include "common/noncopyable.h"
#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/convert.h"
#include "ui/elementid.h"
#include "ui/res/key/menu.h"
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/store/iconstore.h"
#include "ui/res/store/layoutstore.h"
#include "ui/res/store/menudisable.h"
#include "ui/res/store/themestore.h"
#include "ui/res/type/button.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/menu.h"
#include "ui/type.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Res::Store {

// The menu/button/action machinery: loads res/menu/* and res/button/*, builds
// the element-id -> actionKey map, and answers the chrome's menu queries
// (find-by-id, radio highlight, enable/disable). The integration point of the
// resource manager - it composes the other stores (icons for chevrons, theme
// for the theme submenu, layout for popup metrics, locale for display names),
// so ResManager injects them by reference. loadMenus/loadButtons return their
// Changed bit; buildActionMap is a pure rebuild over the loaded menus/buttons.
class MenuStore final : private Common::NonCopyable {
    // Injected collaborators (owned by ResManager; named to match so the loader
    // bodies read naturally). All outlive this store.
    Ui::Res::LocaleManager &   m_localeManager;
    const Store::IconStore &   m_iconStore;
    Store::ThemeStore &        m_themeStore;
    const Store::LayoutStore & m_layoutStore;
    const Ui::Res::ResPath &   m_resPath;

    std::vector<Ui::Res::Type::button_t>  m_buttons;
    std::vector<Ui::Res::Type::menu_t>    m_menus;
    std::unordered_map<id_t, std::string> m_actionMap; // element id -> actionKey (menus + items + buttons)
    // Current value of each stateful (radio) action; a menu item highlights when
    // item.label == m_actionState[item.actionKey](). Providers are host-wired.
    std::unordered_map<std::string, Ui::provider_fn_t> m_actionState;

public:
    MenuStore(Ui::Res::LocaleManager &   localeManager,
              const Store::IconStore &   iconStore,
              Store::ThemeStore &        themeStore,
              const Store::LayoutStore & layoutStore,
              const Ui::Res::ResPath &   resPath)
        : m_localeManager(localeManager)
        , m_iconStore(iconStore)
        , m_themeStore(themeStore)
        , m_layoutStore(layoutStore)
        , m_resPath(resPath)
    {
    }

    [[nodiscard]] const std::vector<Ui::Res::Type::button_t> & buttons() const { return m_buttons; }
    [[nodiscard]] const std::vector<Ui::Res::Type::menu_t> &   menus() const { return m_menus; }

    [[nodiscard]] std::string actionKeyFor(id_t elementId) const
    {
        auto it = m_actionMap.find(elementId);
        return (it != m_actionMap.end()) ? it->second : std::string {};
    }

    // True when this item is the active choice of its stateful action - i.e. its
    // label (the value) equals the action's current value. Computed live.
    [[nodiscard]] bool isActiveItem(const Ui::Res::Type::menu_t & item) const
    {
        if (item.label.empty()) {
            return false;
        }
        auto it = m_actionState.find(item.actionKey);
        return it != m_actionState.end() && it->second() == item.label;
    }

    // Register a host-provided value getter for a stateful (radio) action's menu
    // highlight (SetDisplayMode/SetAmbience live host-side; SwitchTheme internal).
    void setActionValueProvider(const std::string & actionKey, Ui::provider_fn_t provider)
    {
        m_actionState[actionKey] = std::move(provider);
    }

    // Find a menu item by its element ID (searches items and submenus).
    [[nodiscard]] Ui::Res::Type::menu_t findMenuItem(id_t elementId) const
    {
        for (const auto & menu : m_menus) {
            if (menu.id == elementId) {
                return menu;
            }
            for (const auto & item : menu.items) {
                if (item.id == elementId) {
                    return item;
                }
                for (const auto & sub : item.items) {
                    if (sub.id == elementId) {
                        return sub;
                    }
                }
            }
        }
        return {};
    }

    // Resolve a node by its hierarchical key ("View", "View:Theme",
    // "View:Theme:default"). The cross-reload anchor: keys are label-derived so
    // they survive a loadAll() that reassigns numeric ids.
    [[nodiscard]] Ui::Res::Type::menu_t findMenuItemByKey(const Ui::key_t & key) const
    {
        for (const auto & menu : m_menus) {
            if (menu.key == key) {
                return menu;
            }
            for (const auto & item : menu.items) {
                if (item.key == key) {
                    return item;
                }
                for (const auto & sub : item.items) {
                    if (sub.key == key) {
                        return sub;
                    }
                }
            }
        }
        return {};
    }

    // Mark every menu item bound to `actionKey` enabled/disabled (recurses into
    // submenus so radio-group children are caught).
    void setActionEnabled(const std::string & actionKey, bool enabled)
    {
        for (Ui::Res::Type::menu_t & menu : m_menus) {
            setEnabledInTree(actionKey, {}, enabled, menu.items);
        }
    }

    // Per-(action,label) enable/disable, for parameterised actions where many
    // items share one actionKey and the label identifies which preset is gated.
    // label must be non-empty: an empty label is reserved as the match-any
    // wildcard inside the shared walker (that is setActionEnabled's job).
    void setMenuItemEnabled(const std::string & actionKey, const std::string & label, bool enabled)
    {
        for (Ui::Res::Type::menu_t & menu : m_menus) {
            setEnabledInTree(actionKey, label, enabled, menu.items);
        }
    }

    // Disable menu nodes that can do nothing (the recursive collapse lives in
    // Ui::Res::Store::disableUnhandled); this exposes it over the private m_menus.
    void disableUnhandledMenuItems(const Ui::predicate_fn_t & isHandled) { disableUnhandled(m_menus, isHandled); }

    // Recursive by design: builds the nested menu_t tree bottom-up. An iterative
    // builder would hold references into item vectors that reallocate as siblings
    // append (dangling refs). Depth is capped by --menu-max-depth at each descent.
    // NOLINTNEXTLINE(misc-no-recursion)
    Ui::Res::Type::menu_t parseMenuItem(const nlohmann::json & itemJson, int depth) const
    {
        using Ui::Res::Key::MenuKey;
        Ui::Res::Type::menu_t item;
        item.label     = Common::Sanitize::string(itemJson.value(menuKeyName(MenuKey::Label), ""), "menu.label");
        item.actionKey = Common::Sanitize::string(itemJson.value(menuKeyName(MenuKey::Action), ""), "menu.action");
        item.showsThemePreview = (item.actionKey == "SwitchTheme");
        item.shortcut  = Common::Sanitize::string(itemJson.value(menuKeyName(MenuKey::Shortcut), ""), "menu.shortcut");
        item.icon      = Common::Sanitize::filePath(itemJson.value(menuKeyName(MenuKey::Icon), ""), "menu.icon");
        item.separator = itemJson.value(menuKeyName(MenuKey::Separator), false);
        item.enabled   = itemJson.value(menuKeyName(MenuKey::Enabled), true);
        item.visible   = itemJson.value(menuKeyName(MenuKey::Visible), true);

        // "submenus" is polymorphic by design:
        //   array  -> explicit list of submenu items (parsed recursively)
        //   object -> {"auto": "<name>", "action": "<actionKey>"} directs
        //             loadMenus to walk res/submenu/<name>/ and create
        //             one child per file there, each carrying the given
        //             actionKey. Unknown subdir or missing action leaves the
        //             submenu empty - intentional signal for misconfig.
        if (itemJson.contains(menuKeyName(MenuKey::Submenus))) {
            const auto & subs = itemJson[menuKeyName(MenuKey::Submenus)];
            if (subs.is_array()) {
                // Depth cap from layout.json (--menu-max-depth = N allows N levels):
                // bounds the parser recursion - and thereby every downstream
                // menu-tree walk - against a malformed/hostile res JSON. Children
                // past the cap are dropped.
                if (depth + 1 > m_layoutStore.layout().menuMaxDepth) {
                    std::cerr << "[MenuStore] menu depth cap (" << m_layoutStore.layout().menuMaxDepth
                              << ") reached at item: " << item.label << " - submenu ignored" << std::endl;
                } else {
                    for (const auto & subJson : subs) {
                        item.items.emplace_back(parseMenuItem(subJson, depth + 1));
                    }
                }
            } else if (subs.is_object() && subs.contains(menuKeyName(MenuKey::Auto))
                       && subs[menuKeyName(MenuKey::Auto)].is_string()) {
                item.submenu = Common::Sanitize::string(subs[menuKeyName(MenuKey::Auto)].get<std::string>(),
                                                        "menu.submenu");
                // actionKey fired by each auto-generated child (opaque string from
                // JSON). An unknown key simply dispatches to nothing - inert.
                if (subs.contains(menuKeyName(MenuKey::Action)) && subs[menuKeyName(MenuKey::Action)].is_string()) {
                    item.submenuActionKey = Common::Sanitize::string(
                    subs[menuKeyName(MenuKey::Action)].get<std::string>(),
                    "menu.submenuAction");
                }
            }
        }

        const bool hasDialog = itemJson.contains(menuKeyName(MenuKey::Dialog))
                            && itemJson[menuKeyName(MenuKey::Dialog)].is_object();
        if (hasDialog) {
            const auto & dlg    = itemJson[menuKeyName(MenuKey::Dialog)];
            item.dialog.type    = Ui::Res::Type::dialogTypeFromName(dlg.value(menuKeyName(MenuKey::Type), "Info"));
            item.dialog.title   = Common::Sanitize::string(dlg.value(menuKeyName(MenuKey::Title), ""), "dialog.title");
            item.dialog.content = Common::Sanitize::string(dlg.value(menuKeyName(MenuKey::Content), ""),
                                                           "dialog.content");
            item.dialog.link    = Common::Sanitize::string(dlg.value(menuKeyName(MenuKey::Link), ""), "dialog.link");
            item.dialog.icon    = Common::Sanitize::filePath(dlg.value(menuKeyName(MenuKey::Icon), ""), "dialog.icon");
            item.dialog.file    = Common::Sanitize::filePath(dlg.value(menuKeyName(MenuKey::File), ""), "dialog.file");
            item.dialog.width   = Ui::Convert::parseCssNumber(dlg.value(menuKeyName(MenuKey::Width), ""));
            item.dialog.height  = Ui::Convert::parseCssNumber(dlg.value(menuKeyName(MenuKey::Height), ""));
        }

        // No explicit icon? Fall back to a role default from icon-defaults.json.
        // Submenu wins over dialog when both kinds apply. `themes:true` items
        // populate submenus at runtime (empty in JSON) but still want the chevron.
        if (item.icon.empty()) {
            const char * role = nullptr;
            if (!item.items.empty() || !item.submenu.empty()) {
                role = "submenu";
            } else if (hasDialog) {
                role = "dialog";
            }
            if (role != nullptr) {
                if (const auto def = m_iconStore.iconDefault(role); !def.icon.empty()) {
                    item.icon      = def.icon;
                    item.iconPlace = def.place;
                }
            }
        }

        return item;
    }

    Ui::Res::Type::Changed loadMenus(const std::string & dir)
    {
        using Ui::Res::Key::MenuKey;
        if (!Common::dirExists(dir)) {
            return Ui::Res::Type::Changed::None;
        }

        auto oldMenus = m_menus;
        m_menus.clear();
        for (const auto & entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                nlohmann::json j;
                if (!Common::loadJson(entry.path().string(), j)) {
                    continue;
                }

                Ui::Res::Type::menu_t menu;
                menu.label     = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Label), ""), "menu.label");
                menu.order     = j.value(menuKeyName(MenuKey::Order), int16_t {});
                menu.visible   = j.value(menuKeyName(MenuKey::Visible), true);
                menu.icon      = Common::Sanitize::filePath(j.value(menuKeyName(MenuKey::Icon), ""), "menu.icon");
                menu.actionKey = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Action), ""), "menu.action");

                if (j.contains(menuKeyName(MenuKey::Items)) && j[menuKeyName(MenuKey::Items)].is_array()) {
                    for (const auto & itemJson : j[menuKeyName(MenuKey::Items)]) {
                        menu.items.emplace_back(parseMenuItem(itemJson, 1));
                    }
                }

                m_menus.emplace_back(menu);
            }
        }

        // Auto-submenu pass. For each menu item that declared
        // `"submenus": {"auto": "<key>", "action": "<actionKey>"}` in JSON,
        // wire the key-load-action triple:
        //   KEY:    item.submenu (data-driven, from JSON)
        //   LOAD:   per-key C++ call below (data parsing + caching)
        //   ACTION: item.submenuActionKey (data-driven, from JSON)
        // Each branch owns the load step for one key; populateAutoSubmenu
        // is the generic single-file walker shared by anything that
        // follows the JSON-name-as-key convention. Unknown key -> the
        // submenu stays empty as a misconfig signal.
        for (auto & menu : m_menus) {
            for (auto & item : menu.items) {
                if (item.submenu.empty()) {
                    continue;
                }
                item.items.clear();
                if (item.submenu == "theme") {
                    // Paired-file walker. Theme data is loaded eagerly by
                    // loadTheme(current selection) earlier in loadAll; the
                    // populator just enumerates names for the menu.
                    const auto themeNames = m_themeStore.scanThemeNames(m_resPath, m_localeManager);
                    for (const auto & name : themeNames) {
                        Ui::Res::Type::menu_t sub;
                        sub.label     = name; // label is the key/value: get(name) -> display, dispatched to action
                        sub.actionKey = item.submenuActionKey;
                        sub.showsThemePreview = (item.submenuActionKey == "SwitchTheme");
                        sub.enabled           = true;
                        sub.visible           = true;
                        item.items.emplace_back(sub);
                        if (name == "default" && themeNames.size() > 1) {
                            Ui::Res::Type::menu_t sep;
                            sep.separator = true;
                            item.items.emplace_back(sep);
                        }
                    }
                } else {
                    // Generic single-file walker: enumerate names for the menu.
                    // Any matching domain config load is the host's job, done
                    // separately - the fw only builds menu items here.
                    item.items = populateAutoSubmenu(item.submenu, item.submenuActionKey);
                }
            }
        }

        std::sort(m_menus.begin(), m_menus.end(), [](const Ui::Res::Type::menu_t & a, const Ui::Res::Type::menu_t & b) {
            return a.order < b.order;
        });

        // Auto-generate unique IDs (after sort so IDs are deterministic).
        // Each node also gets an authored hierarchical key built from labels
        // (parent:child). Forward-looking identity for addressing; not yet a
        // runtime consumer - the numeric id drives hit-test/dispatch today.
        {
            id_t menuCounter = static_cast<id_t>(Ui::ElementId::MenuBase);
            id_t itemCounter = static_cast<id_t>(Ui::ElementId::ItemBase);
            for (auto & menu : m_menus) {
                menu.id  = menuCounter++;
                menu.key = menu.label;
                for (auto & item : menu.items) {
                    if (!item.separator) {
                        item.id  = itemCounter++;
                        item.key = menu.key + ":" + item.label;
                    }
                    for (auto & sub : item.items) {
                        if (!sub.separator) {
                            sub.id  = itemCounter++;
                            sub.key = item.key + ":" + sub.label;
                        }
                    }
                }
            }
        }

        // Precompute popup content height for each menu
        for (auto & menu : m_menus) {
            int regularItemCount = 0;
            int separatorCount   = 0;
            for (const auto & item : menu.items) {
                if (!item.visible) {
                    continue;
                }
                if (item.separator) {
                    ++separatorCount;
                } else {
                    ++regularItemCount;
                }
            }
            menu.popupHeight = regularItemCount * m_layoutStore.popup().itemHeight
                             + separatorCount
                               * (m_layoutStore.popup().separatorHeight + m_layoutStore.popup().separatorMarginV * 2);
        }

        std::cout << "[MenuStore] Loaded " << m_menus.size() << " menus" << std::endl;

        return m_menus != oldMenus ? Ui::Res::Type::Changed::Menu : Ui::Res::Type::Changed::None;
    }

    Ui::Res::Type::Changed loadButtons(const std::string & dir)
    {
        using Ui::Res::Key::MenuKey;
        if (!Common::dirExists(dir)) {
            return Ui::Res::Type::Changed::None;
        }

        auto oldButtons = m_buttons;
        m_buttons.clear();
        for (const auto & entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                nlohmann::json j;
                if (!Common::loadJson(entry.path().string(), j)) {
                    continue;
                }

                Ui::Res::Type::button_t btn;
                btn.label     = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Label), ""), "button.label");
                btn.actionKey = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Action), ""), "button.action");
                btn.icon      = Common::Sanitize::filePath(j.value(menuKeyName(MenuKey::Icon), ""), "button.icon");
                btn.tooltip   = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Tooltip), ""), "button.tooltip");
                btn.width     = j.value(menuKeyName(MenuKey::Width), fpx_t {});
                btn.height    = j.value(menuKeyName(MenuKey::Height), fpx_t {});
                btn.order     = j.value(menuKeyName(MenuKey::Order), int16_t {});
                btn.enabled   = j.value(menuKeyName(MenuKey::Enabled), true);
                btn.visible   = j.value(menuKeyName(MenuKey::Visible), true);
                m_buttons.emplace_back(btn);
            }
        }

        std::sort(
        m_buttons.begin(),
        m_buttons.end(),
        [](const Ui::Res::Type::button_t & a, const Ui::Res::Type::button_t & b) { return a.order < b.order; });

        // Auto-generate unique IDs (after sort so IDs are deterministic)
        {
            id_t btnCounter = static_cast<id_t>(Ui::ElementId::ButtonBase);
            for (auto & btn : m_buttons) {
                btn.id  = btnCounter++;
                btn.key = btn.label;
            }
        }

        return m_buttons != oldButtons ? Ui::Res::Type::Changed::Button : Ui::Res::Type::Changed::None;
    }

    // Generic single-file auto-submenu walker. `name` is the JSON auto-key,
    // which doubles as the source subdir. Each entry's lowercased name becomes
    // the item label (= key = value passed to the action); the display name is
    // registered in LocaleManager so get(label) resolves it - single owner.
    std::vector<Ui::Res::Type::menu_t> populateAutoSubmenu(const std::string & name, const std::string & actionKey)
    {
        std::vector<Ui::Res::Type::menu_t> items;
        const std::string                  dir = m_resPath.submenuDir(name);
        if (!std::filesystem::exists(dir)) {
            return items;
        }

        struct alignas(128) entry_t {
            int         order = 0;
            std::string key;
            std::string display;
        };
        std::vector<entry_t> entries;

        for (const auto & path : std::filesystem::directory_iterator(dir)) {
            if (path.path().extension() != ".json") {
                continue;
            }
            std::ifstream  f(path.path());
            nlohmann::json j;
            try {
                f >> j;
            } catch (const std::exception &) {
                continue;
            }
            using Ui::Res::Key::MenuKey;
            const std::string display = Common::Sanitize::string(j.value(menuKeyName(MenuKey::Name), std::string {}),
                                                                 "submenu.name");
            if (display.empty()) {
                continue;
            }
            std::string key = display;
            std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            entries.push_back({ j.value(menuKeyName(MenuKey::Order), 0), std::move(key), display });
        }

        std::sort(entries.begin(), entries.end(), [](const entry_t & a, const entry_t & b) {
            if (a.order != b.order) {
                return a.order < b.order;
            }
            return a.key < b.key;
        });

        items.reserve(entries.size());
        for (auto & e : entries) {
            m_localeManager.set(e.key, e.display);
            Ui::Res::Type::menu_t sub;
            sub.actionKey         = actionKey;
            sub.showsThemePreview = (actionKey == "SwitchTheme");
            sub.label             = e.key; // label is the key/value: get(e.key) -> display, dispatched to action
            sub.enabled           = true;
            sub.visible           = true;
            items.emplace_back(std::move(sub));
        }
        return items;
    }

    // Map every actionable element id to its actionKey, so a click (which
    // carries only an id) can dispatch. The radio highlight needs no map -
    // it is a live per-item check (see isActiveItem).
    void buildActionMap()
    {
        m_actionMap.clear();
        auto registerItem = [&](id_t id, const std::string & actionKey) {
            if (!actionKey.empty()) {
                m_actionMap[id] = actionKey;
            }
        };
        for (const auto & menu : m_menus) {
            registerItem(menu.id, menu.actionKey);
            for (const auto & item : menu.items) {
                registerItem(item.id, item.actionKey);
                for (const auto & sub : item.items) {
                    registerItem(sub.id, sub.actionKey);
                }
            }
        }
        for (const auto & btn : m_buttons) {
            registerItem(btn.id, btn.actionKey);
        }
    }

private:
    // Shared walker for the enable/disable setters: marks every item bound to
    // `actionKey` (and, when `label` is non-empty, matching that label too).
    // Iterative worklist walk - visit order does not matter here.
    static void setEnabledInTree(const std::string &                  actionKey,
                                 const std::string &                  label,
                                 bool                                 enabled,
                                 std::vector<Ui::Res::Type::menu_t> & items)
    {
        std::vector<std::reference_wrapper<std::vector<Ui::Res::Type::menu_t>>> pending { items };
        while (!pending.empty()) {
            std::vector<Ui::Res::Type::menu_t> & current = pending.back();
            pending.pop_back();
            for (Ui::Res::Type::menu_t & item : current) {
                if (item.actionKey == actionKey && (label.empty() || item.label == label)) {
                    item.enabled = enabled;
                }
                if (!item.items.empty()) {
                    pending.emplace_back(item.items);
                }
            }
        }
    }
};

} // namespace Ui::Res::Store
