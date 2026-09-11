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

#include "common/json.h"
#include "common/sanitize.h"
#include "nlohmann/json.hpp"
#include "ui/color.h"
#include "ui/convert.h"
#include "ui/res/key/cssprop.h"
#include "ui/res/key/element.h"
#include "ui/res/key/theme.h"
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/font.h"
#include "ui/res/type/theme.h"
#include "ui/type.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Res::Store {

// Loads and holds the active theme (res/submenu/theme/<name>-<mode>.json -> theme_t)
// plus the per-theme preview color cache used by the theme menu. The current
// theme name/mode are owned here too. One of the logical sub-stores ResManager
// composes. loadTheme returns Changed::Theme when the theme actually changed.
class ThemeStore final {
    Ui::Res::Type::theme_t m_theme;
    std::string            m_themeName = "default";
    std::string            m_themeMode = "dark";
    // Theme key -> {dark variant, light variant}; each variant = {cl-main, bg-main}.
    std::unordered_map<std::string, std::pair<Ui::Res::Type::color_pair_t, Ui::Res::Type::color_pair_t>>
    m_themePreviewColors;
    // Keys of the theme file's "host" block: {key, what to do with its value}
    std::vector<std::pair<std::string, Ui::action_fn_t>> m_hostParams;
    // What was last handed to each of them. Opaque strings the framework cannot
    // read, but comparing them is what lets a host-only edit report Changed.
    std::unordered_map<std::string, std::string> m_hostValues;

public:
    // Let a host pull its own values out of every theme file. The framework hands
    // over the raw string; parsing it and knowing what it means stay with the
    // host - the same split registerPersistedJson makes for a host's own session
    // section. Keys live in the theme's "host" block, never in :root, so the two
    // owners cannot collide.
    //
    // Register before the first theme load, the way domain actions are.
    void registerThemeParam(std::string key, Ui::action_fn_t apply)
    {
        m_hostParams.emplace_back(std::move(key), std::move(apply));
    }

    [[nodiscard]] const Ui::Res::Type::theme_t & theme() const { return m_theme; }
    [[nodiscard]] const std::string &            themeName() const { return m_themeName; }
    [[nodiscard]] const std::string &            themeMode() const { return m_themeMode; }
    [[nodiscard]] bool                           isThemeDark() const { return m_themeMode == "dark"; }
    [[nodiscard]] std::string themeIcon() const { return isThemeDark() ? "theme-sun.svg" : "theme-moon.svg"; }

    // Theme preview colors for a given theme key: {dark, light} where each
    // variant is {cl-main, bg-main}. Returns zero-init pair if absent.
    [[nodiscard]] std::pair<Ui::Res::Type::color_pair_t, Ui::Res::Type::color_pair_t>
    themePreviewColors(const std::string & key) const
    {
        const auto it = m_themePreviewColors.find(key);
        return it != m_themePreviewColors.end()
             ? it->second
             : std::pair<Ui::Res::Type::color_pair_t, Ui::Res::Type::color_pair_t> {};
    }

    // Raw setters for session restore: set the value without reloading (loadAll
    // reloads the theme afterwards). Persistence is the host's concern.
    void setThemeNameValue(const std::string & value) { m_themeName = value; }
    void setThemeModeValue(const std::string & value) { m_themeMode = value; }

    // (Re)load the theme file for the current name+mode. Returns Changed::Theme
    // if the theme changed. Called by ResManager::loadAll and the mutators below.
    [[nodiscard]] Ui::Res::Type::Changed loadCurrent(const Ui::Res::ResPath & resPath)
    {
        return loadTheme(resPath.theme(m_themeName, m_themeMode));
    }

    // Runtime changes: set + reload the theme file, returning the Changed bit.
    [[nodiscard]] Ui::Res::Type::Changed setThemeName(const std::string & name, const Ui::Res::ResPath & resPath)
    {
        m_themeName = name;
        return loadCurrent(resPath);
    }

    [[nodiscard]] Ui::Res::Type::Changed switchThemeMode(const Ui::Res::ResPath & resPath)
    {
        m_themeMode = isThemeDark() ? "light" : "dark";
        return loadCurrent(resPath);
    }

    // Scan theme directory for unique theme names (from <name>-<mode>.json pattern).
    // For each theme key, loads both dark and light variants to:
    //  - Register the display name from "name" in Ui::Res::LocaleManager
    //  - Cache {cl-main, bg-main} per variant in m_themePreviewColors
    [[nodiscard]] std::vector<std::string> scanThemeNames(const Ui::Res::ResPath & resPath,
                                                          Ui::Res::LocaleManager & localeManager)
    {
        using Ui::Res::Key::ElementKey;
        using Ui::Res::Key::elementKeyName;
        using Ui::Res::Key::ThemeKey;
        using Ui::Res::Key::themeKeyName;

        m_themePreviewColors.clear();

        std::vector<std::string> names;
        const std::string        themeDir = resPath.submenuDir("theme");
        if (!std::filesystem::exists(themeDir)) {
            return names;
        }

        // Group files by base name so a single map entry holds both variants.
        struct alignas(64) theme_paths_t final {
            std::string dark;
            std::string light;
        };
        std::unordered_map<std::string, theme_paths_t> paths;

        for (const auto & entry : std::filesystem::directory_iterator(themeDir)) {
            if (entry.path().extension() != ".json") {
                continue;
            }
            const std::string stem = entry.path().stem().string();
            for (const auto & suffix : { "-dark", "-light" }) {
                const std::string_view sv(suffix);
                if (stem.size() > sv.size() && stem.ends_with(sv)) {
                    const std::string name = stem.substr(0, stem.size() - sv.size());
                    if (sv == "-dark") {
                        paths[name].dark = entry.path().string();
                    } else {
                        paths[name].light = entry.path().string();
                    }
                }
            }
        }

        // --cl-main and --bg-main are direct hex strings in :root for every
        // shipped theme - no var() chaining - so a flat lookup suffices.
        auto extractRootHex = [](const nlohmann::json & j, ThemeKey key) -> Ui::Color {
            const auto &      root = Common::Json::object(j, elementKeyName(ElementKey::Root));
            const std::string hex  = Common::Json::string(root, themeKeyName(key));
            // Absent or mistyped stays the default-constructed colour, as before -
            // fromHex would answer an empty string with something arbitrary.
            return hex.empty() ? Ui::Color {} : Ui::Color::fromHex(hex);
        };

        const std::string nameKey = themeKeyName(ThemeKey::Name);
        for (auto & [name, files] : paths) {
            Ui::Res::Type::color_pair_t dark {};  // dark  variant: fg=--cl-main, bg=--bg-main
            Ui::Res::Type::color_pair_t light {}; // light variant: fg=--cl-main, bg=--bg-main
            bool                        registeredLocale = false;
            if (!files.dark.empty()) {
                const auto        j       = Common::loadJson(files.dark);
                const std::string display = Common::Sanitize::string(Common::Json::string(j, nameKey), "theme.name");
                if (!display.empty()) {
                    localeManager.set(name, display);
                    registeredLocale = true;
                }
                dark.fg = extractRootHex(j, ThemeKey::ClMain);
                dark.bg = extractRootHex(j, ThemeKey::BgMain);
            }
            if (!files.light.empty()) {
                const auto        j       = Common::loadJson(files.light);
                const std::string display = Common::Sanitize::string(Common::Json::string(j, nameKey), "theme.name");
                if (!registeredLocale && !display.empty()) {
                    localeManager.set(name, display);
                }
                light.fg = extractRootHex(j, ThemeKey::ClMain);
                light.bg = extractRootHex(j, ThemeKey::BgMain);
            }
            m_themePreviewColors[name] = { dark, light };
            names.emplace_back(name);
        }

        // "default" always first, rest sorted alphabetically
        std::sort(names.begin(), names.end(), [](const std::string & a, const std::string & b) {
            if (a == "default") {
                return true;
            }
            if (b == "default") {
                return false;
            }
            return a < b;
        });
        return names;
    }

    Ui::Res::Type::Changed loadTheme(const std::string & file)
    {
        using Ui::Res::Key::CssPropKey;
        using Ui::Res::Key::cssPropKeyName;
        using Ui::Res::Key::ElementKey;
        using Ui::Res::Key::elementKeyName;
        using Ui::Res::Key::ThemeKey;
        using Ui::Res::Key::themeKeyName;

        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return Ui::Res::Type::Changed::None;
        }

        const Ui::Res::Type::theme_t oldTheme = m_theme;

        // Root font families (used as fallbacks for per-element fonts below)
        std::string fontSans;
        std::string fontMono;

        const std::string rootKey = elementKeyName(ElementKey::Root);

        // Build variable map for resolving var() references
        std::unordered_map<std::string, std::string> vars;
        const auto &                                 root = Common::Json::object(j, rootKey);
        for (auto it = root.begin(); it != root.end(); ++it) {
            if (it.value().is_string()) {
                vars[it.key()] = it.value().get<std::string>();
            }
        }

        // Resolve var() in variable values themselves (e.g. "--cl-main": "var(--ground)")
        auto resolveVarValue = [&vars](const std::string & raw) -> std::string {
            const size_t vp = raw.find("var(");
            if (vp != std::string::npos) {
                const size_t ve = raw.find(')', vp);
                if (ve != std::string::npos) {
                    const std::string varName = raw.substr(vp + 4, ve - vp - 4);
                    auto              vit     = vars.find(varName);
                    if (vit != vars.end()) {
                        return vit->second + raw.substr(ve + 1);
                    }
                }
            }
            return raw;
        };

        // Resolve var() references within the variable map itself
        for (auto & [key, value] : vars) {
            value = resolveVarValue(value);
        }

        // Parse :root block for theme variables
        if (j.contains(rootKey) && j[rootKey].is_object()) {
            const auto getVar = [&vars](const std::string & name, const std::string & fallback) -> std::string {
                auto it = vars.find(name);
                return (it != vars.end()) ? it->second : fallback;
            };

            m_theme.main          = { Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClMain), "#990000")),
                                      Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::BgMain), "#636363")) };
            m_theme.second        = { Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClSecond), "#5c0000")),
                                      Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::BgSecond), "#f2f2ff")) };
            m_theme.colorShadow   = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClShadow), "#f1f5f5"));
            m_theme.shadowOpacity = Ui::Convert::parseCssNumber(getVar(themeKeyName(ThemeKey::ShadowOpacity), "0.15"));
            m_theme.colorModel    = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClModel), "#ff9900"));
            m_theme.colorError    = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClError), "#ff00ff"));
            m_theme.colorLoad     = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClLoad), "#aaff00"));
            m_theme.colorInfo     = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClInfo), "#6699cc"));
            m_theme.colorWarn     = Ui::Color::fromHex(getVar(themeKeyName(ThemeKey::ClWarn), "#cc9900"));
            fontSans              = getVar(themeKeyName(ThemeKey::FontSans), "");
            fontMono              = getVar(themeKeyName(ThemeKey::FontMono), "");
        }

        // The host's own block, read in this same pass - so by the time the caller
        // applies the theme diff the host's values are already current, and no
        // second refresh exists to fall out of step with this one. var() is
        // resolved for it too, so a host value can point at the palette.
        //
        // Diffed alongside theme_t, and counted into the same Changed: these keys
        // are not theme_t fields, so without this an edit to the host block alone
        // reaches the host's settings and then stops - nothing downstream is ever
        // told to re-read them, and a reload leaves what is on screen stale.
        bool isHostChanged = false;
        if (!m_hostParams.empty()) {
            const auto & hostBlock = Common::Json::object(j, elementKeyName(ElementKey::Host));
            for (const auto & [key, apply] : m_hostParams) {
                const std::string raw = Common::Json::string(hostBlock, key, "");
                if (raw.empty()) {
                    continue;
                }
                const std::string value = resolveVarValue(raw);
                const auto        it    = m_hostValues.find(key);
                if (it == m_hostValues.end()) {
                    m_hostValues.emplace(key, value);
                    isHostChanged = true;
                } else if (it->second != value) {
                    it->second    = value;
                    isHostChanged = true;
                }
                apply(value);
            }
        }

        // Resolve var(--X) references in a string value
        auto resolveVar = [&vars](const std::string & raw) -> std::string {
            const size_t vp = raw.find("var(");
            if (vp != std::string::npos) {
                const size_t ve = raw.find(')', vp);
                if (ve != std::string::npos) {
                    const std::string varName = raw.substr(vp + 4, ve - vp - 4);
                    auto              vit     = vars.find(varName);
                    if (vit != vars.end()) {
                        return vit->second + raw.substr(ve + 1);
                    }
                }
            }
            return raw;
        };

        // Resolve a CSS color value: replace var(--X), then apply darker()/lighter()
        auto resolveColor = [&resolveVar](const std::string & raw, const Ui::Color & fallback) -> Ui::Color {
            if (raw.empty()) {
                return fallback;
            }
            if (raw == "inherit") {
                return Ui::Color::Inherit();
            }
            const std::string val = resolveVar(raw);
            // Check for darker(N) / lighter(N) modifier
            Ui::Color    base;
            const size_t hashPos = val.find('#');
            if (hashPos != std::string::npos) {
                const size_t colorEnd = val.find(' ', hashPos);
                if (colorEnd == std::string::npos) {
                    return Ui::Color::fromHex(val.substr(hashPos));
                }
                base                  = Ui::Color::fromHex(val.substr(hashPos, colorEnd - hashPos));
                const std::string mod = val.substr(colorEnd + 1);
                if (mod.rfind("darker(", 0) == 0) {
                    const float v = std::stof(mod.substr(7));
                    return base.Darker(v);
                }
                if (mod.rfind("lighter(", 0) == 0) {
                    const float v = std::stof(mod.substr(8));
                    return base.Lighter(v);
                }
                return base;
            }
            return fallback;
        };

        // Worker: read "background" or "color" from a named JSON block (raw string keys).
        auto blockColorBy = [&](const std::string & block, const std::string & prop) -> Ui::Color {
            // Empty covers absent block, absent property and a mistyped value
            // alike - all three already resolved to colorError.
            const std::string value = Common::Json::string(Common::Json::object(j, block), prop);
            return value.empty() ? m_theme.colorError : resolveColor(value, m_theme.colorError);
        };
        // Enum-typed overload: the call sites reference fixed selectors/properties,
        // so they pass ElementKey/CssPropKey and the spelling stays single-sourced.
        auto blockColor = [&blockColorBy](ElementKey block, CssPropKey prop) -> Ui::Color {
            return blockColorBy(elementKeyName(block), cssPropKeyName(prop));
        };

        // Regions {fg, bg}
        m_theme.topMenu       = { blockColor(ElementKey::TopMenu, CssPropKey::Color),
                                  blockColor(ElementKey::TopMenu, CssPropKey::Background) };
        m_theme.dropdown      = { m_theme.colorError, blockColor(ElementKey::TopMenuDropdown, CssPropKey::Background) };
        m_theme.leftToolbar   = { blockColor(ElementKey::LeftToolbar, CssPropKey::Color),
                                  blockColor(ElementKey::LeftToolbar, CssPropKey::Background) };
        m_theme.rightToolbar  = { blockColor(ElementKey::RightToolbar, CssPropKey::Color),
                                  blockColor(ElementKey::RightToolbar, CssPropKey::Background) };
        m_theme.workspace     = { blockColor(ElementKey::Workspace, CssPropKey::Color),
                                  blockColor(ElementKey::Workspace, CssPropKey::Background) };
        m_theme.workspaceTabs = { blockColor(ElementKey::WorkspaceTabs, CssPropKey::Color),
                                  blockColor(ElementKey::WorkspaceTabs, CssPropKey::Background) };

        // Interactive elements: normal {fg, bg}, hover {fg, bg}, active {fg, bg}
        m_theme.topMenuButton         = m_theme.topMenu;
        m_theme.topMenuButtonHover    = { blockColor(ElementKey::TopMenuButtonLabelHover, CssPropKey::Color),
                                          blockColor(ElementKey::TopMenuButtonLabelHover, CssPropKey::Background) };
        m_theme.topMenuButtonActive   = { blockColor(ElementKey::TopMenuButtonLabelActive, CssPropKey::Color),
                                          blockColor(ElementKey::TopMenuButtonLabelActive, CssPropKey::Background) };
        m_theme.menuItem              = { blockColor(ElementKey::TopMenuItem, CssPropKey::Color),
                                          blockColor(ElementKey::TopMenuItem, CssPropKey::Background) };
        m_theme.menuItemHover         = { blockColor(ElementKey::TopMenuItemHover, CssPropKey::Color),
                                          blockColor(ElementKey::TopMenuItemHover, CssPropKey::Background) };
        m_theme.menuItemActive        = { blockColor(ElementKey::TopMenuItemActive, CssPropKey::Color),
                                          blockColor(ElementKey::TopMenuItemActive, CssPropKey::Background) };
        m_theme.button                = { blockColor(ElementKey::Button, CssPropKey::Color),
                                          blockColor(ElementKey::Button, CssPropKey::Background) };
        m_theme.workspaceTab          = { blockColor(ElementKey::WorkspaceTab, CssPropKey::Color),
                                          blockColor(ElementKey::WorkspaceTab, CssPropKey::Background) };
        m_theme.workspaceTabHover     = { blockColor(ElementKey::WorkspaceTabHover, CssPropKey::Color),
                                          blockColor(ElementKey::WorkspaceTabHover, CssPropKey::Background) };
        m_theme.workspaceTabActive    = { blockColor(ElementKey::WorkspaceTabActive, CssPropKey::Color),
                                          blockColor(ElementKey::WorkspaceTabActive, CssPropKey::Background) };
        m_theme.tabClose              = { blockColor(ElementKey::WorkspaceTabClose, CssPropKey::Color),
                                          blockColor(ElementKey::WorkspaceTabClose, CssPropKey::Background) };
        m_theme.tabCloseHover         = { blockColor(ElementKey::WorkspaceTabCloseHover, CssPropKey::Color),
                                          blockColor(ElementKey::WorkspaceTabCloseHover, CssPropKey::Background) };
        m_theme.dialog                = { blockColor(ElementKey::Dialog, CssPropKey::Color),
                                          blockColor(ElementKey::Dialog, CssPropKey::Background) };
        const auto &      dialogTheme = Common::Json::object(j, elementKeyName(ElementKey::Dialog));
        const std::string lineHeight  = Common::Json::string(dialogTheme, cssPropKeyName(CssPropKey::LineHeight));
        // Only override when the theme actually states one: parseCssNumber("")
        // is 0, which would collapse every dialog line.
        if (!lineHeight.empty()) {
            m_theme.dialogLineHeight = Ui::Convert::parseCssNumber(lineHeight);
        }
        // Same guard as line-height: an absent key parses to 0, which would
        // make an armed toolbar button indistinguishable from an idle one
        const auto &      buttonTheme    = Common::Json::object(j, elementKeyName(ElementKey::Button));
        const std::string activeContrast = Common::Json::string(buttonTheme,
                                                                cssPropKeyName(CssPropKey::ActiveContrast));
        if (!activeContrast.empty()) {
            m_theme.buttonActiveContrast = Ui::Convert::parseCssNumber(activeContrast);
        }
        m_theme.dialogTitleColor    = blockColor(ElementKey::DialogTitle, CssPropKey::Color);
        m_theme.dialogLinkColor     = blockColor(ElementKey::DialogLink, CssPropKey::Color);
        m_theme.dialogLinkVisited   = blockColor(ElementKey::DialogLinkVisited, CssPropKey::Color);
        m_theme.dialogButton        = { blockColor(ElementKey::DialogButton, CssPropKey::Color),
                                        blockColor(ElementKey::DialogButton, CssPropKey::Background) };
        m_theme.dialogButtonHover   = { blockColor(ElementKey::DialogButtonHover, CssPropKey::Color),
                                        blockColor(ElementKey::DialogButtonHover, CssPropKey::Background) };
        m_theme.dialogButtonActive  = { blockColor(ElementKey::DialogButtonActive, CssPropKey::Color),
                                        blockColor(ElementKey::DialogButtonActive, CssPropKey::Background) };
        m_theme.dialogButtonPrimary = { blockColor(ElementKey::DialogButtonPrimary, CssPropKey::Color),
                                        blockColor(ElementKey::DialogButtonPrimary, CssPropKey::Background) };
        m_theme.scrollbarTrack      = blockColor(ElementKey::Scrollbar, CssPropKey::Background);
        m_theme.scrollbarThumb      = blockColor(ElementKey::ScrollbarThumb, CssPropKey::Background);
        m_theme.scrollbarThumbHover = blockColor(ElementKey::ScrollbarThumbHover, CssPropKey::Background);
        m_theme.dialogClose         = { blockColor(ElementKey::DialogClose, CssPropKey::Color),
                                        blockColor(ElementKey::DialogClose, CssPropKey::Background) };
        m_theme.dialogCloseHover    = { blockColor(ElementKey::DialogCloseHover, CssPropKey::Color),
                                        blockColor(ElementKey::DialogCloseHover, CssPropKey::Background) };
        m_theme.dialogCloseActive   = { blockColor(ElementKey::DialogCloseActive, CssPropKey::Color),
                                        blockColor(ElementKey::DialogCloseActive, CssPropKey::Background) };
        m_theme.tabArrow            = { blockColor(ElementKey::WorkspaceTabArrow, CssPropKey::Color),
                                        blockColor(ElementKey::WorkspaceTabArrow, CssPropKey::Background) };
        m_theme.statusBar           = { blockColor(ElementKey::StatusBar, CssPropKey::Color),
                                        blockColor(ElementKey::StatusBar, CssPropKey::Background) };
        m_theme.statusBarActive     = { blockColor(ElementKey::StatusBarActive, CssPropKey::Color),
                                        blockColor(ElementKey::StatusBarActive, CssPropKey::Background) };

        // Standalone colors
        m_theme.menuItemDisabledColor = blockColor(ElementKey::TopMenuItemDisabled, CssPropKey::Color);
        m_theme.separatorColor        = blockColor(ElementKey::TopMenuSeparator, CssPropKey::Background);
        m_theme.shortcutColor         = blockColor(ElementKey::TopMenuItemShortcut, CssPropKey::Color);
        m_theme.shortcutHoverColor    = blockColor(ElementKey::TopMenuItemShortcutHover, CssPropKey::Color);

        // Worker: read a Ui::Res::Type::font_t {family, size, weight} from a named JSON block.
        auto blockFontBy = [&](const std::string &       block,
                               const std::string &       familyFallback,
                               int                       sizeFallback,
                               Ui::Res::Type::FontWeight weightFallback) -> Ui::Res::Type::font_t {
            Ui::Res::Type::font_t f;
            f.family = familyFallback;
            f.size   = sizeFallback;
            f.weight = weightFallback;
            // Each field keeps its fallback unless the theme states one: an empty
            // family would erase the font, and parseCssInt("") is 0.
            const auto & obj = Common::Json::object(j, block);

            const std::string family = Common::Json::string(obj, cssPropKeyName(CssPropKey::FontFamily));
            if (!family.empty()) {
                f.family = resolveVar(Common::Sanitize::string(family, "theme.font-family"));
            }

            const std::string size = Common::Json::string(obj, cssPropKeyName(CssPropKey::FontSize));
            if (!size.empty()) {
                f.size = Ui::Convert::parseCssInt(size);
            }

            const std::string weight = Common::Json::string(obj, cssPropKeyName(CssPropKey::FontWeight));
            if (!weight.empty()) {
                f.weight = (weight == "bold") ? Ui::Res::Type::FontWeight::Bold : Ui::Res::Type::FontWeight::Regular;
            }
            return f;
        };
        // Enum-typed overload of blockFont: fixed selectors pass ElementKey.
        auto blockFont = [&blockFontBy](ElementKey                block,
                                        const std::string &       familyFallback,
                                        int                       sizeFallback,
                                        Ui::Res::Type::FontWeight weightFallback) -> Ui::Res::Type::font_t {
            return blockFontBy(elementKeyName(block), familyFallback, sizeFallback, weightFallback);
        };

        // Per-element fonts
        m_theme.topMenuFont      = blockFont(ElementKey::TopMenu, fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.menuItemFont     = blockFont(ElementKey::TopMenuItem, fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.shortcutFont     = blockFont(ElementKey::TopMenuItemShortcut,
                                         fontMono,
                                         16,
                                         Ui::Res::Type::FontWeight::Regular);
        m_theme.leftToolbarFont  = blockFont(ElementKey::LeftToolbar, fontSans, 24, Ui::Res::Type::FontWeight::Bold);
        m_theme.buttonFont       = blockFont(ElementKey::Button, fontSans, 24, Ui::Res::Type::FontWeight::Bold);
        m_theme.rightToolbarFont = blockFont(ElementKey::RightToolbar,
                                             fontSans,
                                             24,
                                             Ui::Res::Type::FontWeight::Regular);
        m_theme.statusBarFont    = blockFont(ElementKey::StatusBar, fontMono, 14, Ui::Res::Type::FontWeight::Regular);
        m_theme.workspaceTabFont = blockFont(ElementKey::WorkspaceTab,
                                             fontSans,
                                             14,
                                             Ui::Res::Type::FontWeight::Regular);
        m_theme.dialogFont       = blockFont(ElementKey::Dialog, fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.dialogTitleFont  = blockFont(ElementKey::DialogTitle, fontSans, 24, Ui::Res::Type::FontWeight::Bold);

        // workspace-tab:active font-weight
        {
            const Ui::Res::Type::font_t active = blockFont(ElementKey::WorkspaceTabActive,
                                                           "",
                                                           0,
                                                           Ui::Res::Type::FontWeight::Bold);
            m_theme.workspaceTabActiveWeight   = active.weight;
        }

        // Dock primitive. One theme shared by all dock instances; grip is
        // the inkscape-style 3-dot resize handle on the viewport-facing edge.
        m_theme.dock.background       = { blockColor(ElementKey::Dock, CssPropKey::Color),
                                          blockColor(ElementKey::Dock, CssPropKey::Background) };
        m_theme.dock.grip             = { blockColor(ElementKey::DockGrip, CssPropKey::Color),
                                          blockColor(ElementKey::DockGrip, CssPropKey::Background) };
        m_theme.dock.gripHover        = { blockColor(ElementKey::DockGripHover, CssPropKey::Color),
                                          blockColor(ElementKey::DockGripHover, CssPropKey::Background) };
        m_theme.dock.gripActive       = { blockColor(ElementKey::DockGripActive, CssPropKey::Color),
                                          blockColor(ElementKey::DockGripActive, CssPropKey::Background) };
        m_theme.dock.sliderTrack      = { blockColor(ElementKey::DockSliderTrack, CssPropKey::Color),
                                          blockColor(ElementKey::DockSliderTrack, CssPropKey::Background) };
        m_theme.dock.sliderThumb      = { blockColor(ElementKey::DockSliderThumb, CssPropKey::Color),
                                          blockColor(ElementKey::DockSliderThumb, CssPropKey::Background) };
        m_theme.dock.sliderThumbHover = { blockColor(ElementKey::DockSliderThumbHover, CssPropKey::Color),
                                          blockColor(ElementKey::DockSliderThumbHover, CssPropKey::Background) };
        m_theme.dock.separator        = blockColor(ElementKey::DockSeparator, CssPropKey::Background);

        std::cout << "[Theme] loaded: bg-main=" << m_theme.main.bg.toHex()
                  << ", bg-second=" << m_theme.second.bg.toHex() << ", ws-tab-bg=" << m_theme.workspaceTab.bg.toHex()
                  << ", ws-tab-fg=" << m_theme.workspaceTab.fg.toHex() << std::endl;

        return (m_theme == oldTheme && !isHostChanged) ? Ui::Res::Type::Changed::None : Ui::Res::Type::Changed::Theme;
    }
};

} // namespace Ui::Res::Store
