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
#include "ui/res/localemanager.h"
#include "ui/res/respath.h"
#include "ui/res/type/changed.h"
#include "ui/res/type/colorpair.h"
#include "ui/res/type/font.h"
#include "ui/res/type/theme.h"

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

public:
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
        auto extractRootHex = [](const nlohmann::json & j, const std::string & key) -> Ui::Color {
            if (j.contains(":root") && j[":root"].is_object()) {
                const auto & root = j[":root"];
                if (root.contains(key) && root[key].is_string()) {
                    return Ui::Color::fromHex(root[key].get<std::string>());
                }
            }
            return {};
        };

        for (auto & [name, files] : paths) {
            Ui::Res::Type::color_pair_t dark {};  // dark  variant: fg=--cl-main, bg=--bg-main
            Ui::Res::Type::color_pair_t light {}; // light variant: fg=--cl-main, bg=--bg-main
            bool                        registeredLocale = false;
            if (!files.dark.empty()) {
                nlohmann::json j;
                if (Common::loadJson(files.dark, j)) {
                    if (j.contains("name") && j["name"].is_string()) {
                        localeManager.set(name, Common::Sanitize::string(j["name"].get<std::string>(), "theme.name"));
                        registeredLocale = true;
                    }
                    dark.fg = extractRootHex(j, "--cl-main");
                    dark.bg = extractRootHex(j, "--bg-main");
                }
            }
            if (!files.light.empty()) {
                nlohmann::json j;
                if (Common::loadJson(files.light, j)) {
                    if (!registeredLocale && j.contains("name") && j["name"].is_string()) {
                        localeManager.set(name, Common::Sanitize::string(j["name"].get<std::string>(), "theme.name"));
                    }
                    light.fg = extractRootHex(j, "--cl-main");
                    light.bg = extractRootHex(j, "--bg-main");
                }
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
        nlohmann::json j;
        if (!Common::loadJson(file, j)) {
            return Ui::Res::Type::Changed::None;
        }

        const Ui::Res::Type::theme_t oldTheme = m_theme;

        // Root font families (used as fallbacks for per-element fonts below)
        std::string fontSans;
        std::string fontMono;

        // Build variable map for resolving var() references
        std::unordered_map<std::string, std::string> vars;
        if (j.contains(":root") && j[":root"].is_object()) {
            for (auto it = j[":root"].begin(); it != j[":root"].end(); ++it) {
                if (it.value().is_string()) {
                    vars[it.key()] = it.value().get<std::string>();
                }
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
        if (j.contains(":root") && j[":root"].is_object()) {
            const auto getVar = [&vars](const std::string & name, const std::string & fallback) -> std::string {
                auto it = vars.find(name);
                return (it != vars.end()) ? it->second : fallback;
            };

            m_theme.main          = { Ui::Color::fromHex(getVar("--cl-main", "#990000")),
                                      Ui::Color::fromHex(getVar("--bg-main", "#636363")) };
            m_theme.second        = { Ui::Color::fromHex(getVar("--cl-second", "#5c0000")),
                                      Ui::Color::fromHex(getVar("--bg-second", "#f2f2ff")) };
            m_theme.colorShadow   = Ui::Color::fromHex(getVar("--cl-shadow", "#f1f5f5"));
            m_theme.shadowOpacity = Ui::Convert::parseCssNumber(getVar("--shadow-opacity", "0.15"));
            m_theme.colorModel    = Ui::Color::fromHex(getVar("--cl-model", "#ff9900"));
            m_theme.colorError    = Ui::Color::fromHex(getVar("--cl-error", "#ff00ff"));
            m_theme.colorLoad     = Ui::Color::fromHex(getVar("--cl-load", "#aaff00"));
            m_theme.colorInfo     = Ui::Color::fromHex(getVar("--cl-info", "#336699"));
            m_theme.colorWarn     = Ui::Color::fromHex(getVar("--cl-warn", "#ff9933"));
            fontSans              = getVar("--font-sans", "");
            fontMono              = getVar("--font-mono", "");
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

        // Helper: read "background" or "color" from a named JSON block
        auto blockColor = [&](const std::string & block, const std::string & prop) -> Ui::Color {
            if (j.contains(block) && j[block].is_object()) {
                const auto & obj = j[block];
                if (obj.contains(prop) && obj[prop].is_string()) {
                    return resolveColor(obj[prop].get<std::string>(), m_theme.colorError);
                }
            }
            return m_theme.colorError;
        };

        // Regions {fg, bg}
        m_theme.topMenu       = { blockColor("top-menu", "color"), blockColor("top-menu", "background") };
        m_theme.dropdown      = { m_theme.colorError, blockColor("top-menu-dropdown", "background") };
        m_theme.leftToolbar   = { blockColor("left-toolbar", "color"), blockColor("left-toolbar", "background") };
        m_theme.rightToolbar  = { blockColor("right-toolbar", "color"), blockColor("right-toolbar", "background") };
        m_theme.workspace     = { blockColor("workspace", "color"), blockColor("workspace", "background") };
        m_theme.workspaceTabs = { blockColor("workspace-tabs", "color"), blockColor("workspace-tabs", "background") };

        // Interactive elements: normal {fg, bg}, hover {fg, bg}, active {fg, bg}
        m_theme.topMenuButton       = m_theme.topMenu;
        m_theme.topMenuButtonHover  = { blockColor("top-menu-button-label:hover", "color"),
                                        blockColor("top-menu-button-label:hover", "background") };
        m_theme.topMenuButtonActive = { blockColor("top-menu-button-label:active", "color"),
                                        blockColor("top-menu-button-label:active", "background") };
        m_theme.menuItem          = { blockColor("top-menu-item", "color"), blockColor("top-menu-item", "background") };
        m_theme.menuItemHover     = { blockColor("top-menu-item:hover", "color"),
                                      blockColor("top-menu-item:hover", "background") };
        m_theme.menuItemActive    = { blockColor("top-menu-item:active", "color"),
                                      blockColor("top-menu-item:active", "background") };
        m_theme.button            = { blockColor("button", "color"), blockColor("button", "background") };
        m_theme.workspaceTab      = { blockColor("workspace-tab", "color"), blockColor("workspace-tab", "background") };
        m_theme.workspaceTabHover = { blockColor("workspace-tab:hover", "color"),
                                      blockColor("workspace-tab:hover", "background") };
        m_theme.workspaceTabActive = { blockColor("workspace-tab:active", "color"),
                                       blockColor("workspace-tab:active", "background") };
        m_theme.tabClose           = { blockColor("workspace-tab-close", "color"),
                                       blockColor("workspace-tab-close", "background") };
        m_theme.tabCloseHover      = { blockColor("workspace-tab-close:hover", "color"),
                                       blockColor("workspace-tab-close:hover", "background") };
        m_theme.dialog             = { blockColor("dialog", "color"), blockColor("dialog", "background") };
        if (j.contains("dialog") && j["dialog"].is_object()) {
            const auto & dlgTheme = j["dialog"];
            if (dlgTheme.contains("line-height") && dlgTheme["line-height"].is_string()) {
                m_theme.dialogLineHeight = Ui::Convert::parseCssNumber(dlgTheme["line-height"].get<std::string>());
            }
        }
        m_theme.dialogTitleColor  = blockColor("dialog-title", "color");
        m_theme.dialogLinkColor   = blockColor("dialog-link", "color");
        m_theme.dialogLinkVisited = blockColor("dialog-link:visited", "color");
        m_theme.dialogButton      = { blockColor("dialog-button", "color"), blockColor("dialog-button", "background") };
        m_theme.dialogButtonHover = { blockColor("dialog-button:hover", "color"),
                                      blockColor("dialog-button:hover", "background") };
        m_theme.dialogButtonActive        = { blockColor("dialog-button:active", "color"),
                                              blockColor("dialog-button:active", "background") };
        m_theme.dialogButtonPrimary       = { blockColor("dialog-button:primary", "color"),
                                              blockColor("dialog-button:primary", "background") };
        m_theme.dialogScrollbarTrack      = blockColor("dialog-scrollbar", "background");
        m_theme.dialogScrollbarThumb      = blockColor("dialog-scrollbar-thumb", "background");
        m_theme.dialogScrollbarThumbHover = blockColor("dialog-scrollbar-thumb:hover", "background");
        m_theme.dialogClose       = { blockColor("dialog-close", "color"), blockColor("dialog-close", "background") };
        m_theme.dialogCloseHover  = { blockColor("dialog-close:hover", "color"),
                                      blockColor("dialog-close:hover", "background") };
        m_theme.dialogCloseActive = { blockColor("dialog-close:active", "color"),
                                      blockColor("dialog-close:active", "background") };
        m_theme.tabArrow          = { blockColor("workspace-tab-arrow", "color"),
                                      blockColor("workspace-tab-arrow", "background") };
        m_theme.statusBar         = { blockColor("status-bar", "color"), blockColor("status-bar", "background") };
        m_theme.statusBarActive   = { blockColor("status-bar:active", "color"),
                                      blockColor("status-bar:active", "background") };

        // Standalone colors
        m_theme.menuItemDisabledColor = blockColor("top-menu-item:disabled", "color");
        m_theme.separatorColor        = blockColor("top-menu-separator", "background");
        m_theme.shortcutColor         = blockColor("top-menu-item-shortcut", "color");
        m_theme.shortcutHoverColor    = blockColor("top-menu-item-shortcut:hover", "color");

        // Helper: read a Ui::Res::Type::font_t {family, size, weight} from a named JSON block
        auto blockFont = [&](const std::string &       block,
                             const std::string &       familyFallback,
                             int                       sizeFallback,
                             Ui::Res::Type::FontWeight weightFallback) -> Ui::Res::Type::font_t {
            Ui::Res::Type::font_t f;
            f.family = familyFallback;
            f.size   = sizeFallback;
            f.weight = weightFallback;
            if (j.contains(block) && j[block].is_object()) {
                const auto & obj = j[block];
                if (obj.contains("font-family") && obj["font-family"].is_string()) {
                    f.family = resolveVar(
                    Common::Sanitize::string(obj["font-family"].get<std::string>(), "theme.font-family"));
                }
                if (obj.contains("font-size") && obj["font-size"].is_string()) {
                    f.size = Ui::Convert::parseCssInt(obj["font-size"].get<std::string>());
                }
                if (obj.contains("font-weight") && obj["font-weight"].is_string()) {
                    const std::string w = obj["font-weight"].get<std::string>();
                    f.weight = (w == "bold") ? Ui::Res::Type::FontWeight::Bold : Ui::Res::Type::FontWeight::Regular;
                }
            }
            return f;
        };

        // Per-element fonts
        m_theme.topMenuFont     = blockFont("top-menu", fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.menuItemFont    = blockFont("top-menu-item", fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.shortcutFont    = blockFont("top-menu-item-shortcut", fontMono, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.leftToolbarFont = blockFont("left-toolbar", fontSans, 24, Ui::Res::Type::FontWeight::Bold);
        m_theme.buttonFont      = blockFont("button", fontSans, 24, Ui::Res::Type::FontWeight::Bold);
        m_theme.rightToolbarFont = blockFont("right-toolbar", fontSans, 24, Ui::Res::Type::FontWeight::Regular);
        m_theme.statusBarFont    = blockFont("status-bar", fontMono, 14, Ui::Res::Type::FontWeight::Regular);
        m_theme.workspaceTabFont = blockFont("workspace-tab", fontSans, 14, Ui::Res::Type::FontWeight::Regular);
        m_theme.dialogFont       = blockFont("dialog", fontSans, 16, Ui::Res::Type::FontWeight::Regular);
        m_theme.dialogTitleFont  = blockFont("dialog-title", fontSans, 24, Ui::Res::Type::FontWeight::Bold);

        // workspace-tab:active font-weight
        {
            const Ui::Res::Type::font_t active = blockFont("workspace-tab:active",
                                                           "",
                                                           0,
                                                           Ui::Res::Type::FontWeight::Bold);
            m_theme.workspaceTabActiveWeight   = active.weight;
        }

        // Dock primitive. One theme shared by all dock instances; grip is
        // the inkscape-style 3-dot resize handle on the viewport-facing edge.
        m_theme.dock.background = { blockColor("dock", "color"), blockColor("dock", "background") };
        m_theme.dock.grip       = { blockColor("dock-grip", "color"), blockColor("dock-grip", "background") };
        m_theme.dock.gripHover  = { blockColor("dock-grip:hover", "color"),
                                    blockColor("dock-grip:hover", "background") };
        m_theme.dock.gripActive = { blockColor("dock-grip:active", "color"),
                                    blockColor("dock-grip:active", "background") };
        m_theme.dock.separator  = blockColor("dock-separator", "background");

        std::cout << "[Theme] loaded: bg-main=" << m_theme.main.bg.toHex()
                  << ", bg-second=" << m_theme.second.bg.toHex() << ", ws-tab-bg=" << m_theme.workspaceTab.bg.toHex()
                  << ", ws-tab-fg=" << m_theme.workspaceTab.fg.toHex() << std::endl;

        return m_theme == oldTheme ? Ui::Res::Type::Changed::None : Ui::Res::Type::Changed::Theme;
    }
};

} // namespace Ui::Res::Store
