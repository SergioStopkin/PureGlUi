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

#include <string>

namespace Ui::Res::Key {

// CSS block SELECTORS shared by res/layout.json (LayoutStore) and the theme
// files (ThemeStore). Both stores key off the SAME selectors - layout reads
// geometry from a block, the theme reads colors/fonts from the same block - so
// the spelling lives here exactly once and the two stores can never disagree.
// Spellings stay in CSS kebab / pseudo-class convention (never re-spelled),
// resolved through elementKeyName(). Property names inside a block live in
// CssPropKey; the per-store CSS custom-property variables live in LayoutKey /
// ThemeKey.
enum class ElementKey : unsigned char {
    Root, // :root variable block

    TopMenu,                  // top-menu
    TopMenuDropdown,          // top-menu-dropdown
    TopMenuItem,              // top-menu-item
    TopMenuItemHover,         // top-menu-item:hover
    TopMenuItemActive,        // top-menu-item:active
    TopMenuItemDisabled,      // top-menu-item:disabled
    TopMenuItemIcon,          // top-menu-item-icon
    TopMenuItemShortcut,      // top-menu-item-shortcut
    TopMenuItemShortcutHover, // top-menu-item-shortcut:hover
    TopMenuSeparator,         // top-menu-separator
    TopMenuButtonLabel,       // top-menu-button-label
    TopMenuButtonLabelHover,  // top-menu-button-label:hover
    TopMenuButtonLabelActive, // top-menu-button-label:active

    LeftToolbar,     // left-toolbar
    RightToolbar,    // right-toolbar
    StatusBar,       // status-bar
    StatusBarActive, // status-bar:active

    Workspace,              // workspace
    WorkspaceTabs,          // workspace-tabs
    WorkspaceTab,           // workspace-tab
    WorkspaceTabHover,      // workspace-tab:hover
    WorkspaceTabActive,     // workspace-tab:active
    WorkspaceTabClose,      // workspace-tab-close
    WorkspaceTabCloseHover, // workspace-tab-close:hover
    WorkspaceTabArrow,      // workspace-tab-arrow

    Button,       // button
    ThemePreview, // theme-preview

    Dialog,                    // dialog
    DialogTitle,               // dialog-title
    DialogText,                // dialog-text
    DialogIcon,                // dialog-icon
    DialogClose,               // dialog-close
    DialogCloseHover,          // dialog-close:hover
    DialogCloseActive,         // dialog-close:active
    DialogButton,              // dialog-button
    DialogButtonHover,         // dialog-button:hover
    DialogButtonActive,        // dialog-button:active
    DialogButtonPrimary,       // dialog-button:primary
    DialogLink,                // dialog-link
    DialogLinkVisited,         // dialog-link:visited
    DialogScrollbar,           // dialog-scrollbar
    DialogScrollbarHover,      // dialog-scrollbar:hover
    DialogScrollbarThumb,      // dialog-scrollbar-thumb
    DialogScrollbarThumbHover, // dialog-scrollbar-thumb:hover

    Dock,           // dock
    DockDefaults,   // dock-defaults
    DockGrip,       // dock-grip
    DockGripHover,  // dock-grip:hover
    DockGripActive, // dock-grip:active
    DockSeparator,  // dock-separator
};

[[nodiscard]] inline std::string elementKeyName(ElementKey key)
{
    switch (key) {
    case ElementKey::Root: return ":root";

    case ElementKey::TopMenu: return "top-menu";
    case ElementKey::TopMenuDropdown: return "top-menu-dropdown";
    case ElementKey::TopMenuItem: return "top-menu-item";
    case ElementKey::TopMenuItemHover: return "top-menu-item:hover";
    case ElementKey::TopMenuItemActive: return "top-menu-item:active";
    case ElementKey::TopMenuItemDisabled: return "top-menu-item:disabled";
    case ElementKey::TopMenuItemIcon: return "top-menu-item-icon";
    case ElementKey::TopMenuItemShortcut: return "top-menu-item-shortcut";
    case ElementKey::TopMenuItemShortcutHover: return "top-menu-item-shortcut:hover";
    case ElementKey::TopMenuSeparator: return "top-menu-separator";
    case ElementKey::TopMenuButtonLabel: return "top-menu-button-label";
    case ElementKey::TopMenuButtonLabelHover: return "top-menu-button-label:hover";
    case ElementKey::TopMenuButtonLabelActive: return "top-menu-button-label:active";

    case ElementKey::LeftToolbar: return "left-toolbar";
    case ElementKey::RightToolbar: return "right-toolbar";
    case ElementKey::StatusBar: return "status-bar";
    case ElementKey::StatusBarActive: return "status-bar:active";

    case ElementKey::Workspace: return "workspace";
    case ElementKey::WorkspaceTabs: return "workspace-tabs";
    case ElementKey::WorkspaceTab: return "workspace-tab";
    case ElementKey::WorkspaceTabHover: return "workspace-tab:hover";
    case ElementKey::WorkspaceTabActive: return "workspace-tab:active";
    case ElementKey::WorkspaceTabClose: return "workspace-tab-close";
    case ElementKey::WorkspaceTabCloseHover: return "workspace-tab-close:hover";
    case ElementKey::WorkspaceTabArrow: return "workspace-tab-arrow";

    case ElementKey::Button: return "button";
    case ElementKey::ThemePreview: return "theme-preview";

    case ElementKey::Dialog: return "dialog";
    case ElementKey::DialogTitle: return "dialog-title";
    case ElementKey::DialogText: return "dialog-text";
    case ElementKey::DialogIcon: return "dialog-icon";
    case ElementKey::DialogClose: return "dialog-close";
    case ElementKey::DialogCloseHover: return "dialog-close:hover";
    case ElementKey::DialogCloseActive: return "dialog-close:active";
    case ElementKey::DialogButton: return "dialog-button";
    case ElementKey::DialogButtonHover: return "dialog-button:hover";
    case ElementKey::DialogButtonActive: return "dialog-button:active";
    case ElementKey::DialogButtonPrimary: return "dialog-button:primary";
    case ElementKey::DialogLink: return "dialog-link";
    case ElementKey::DialogLinkVisited: return "dialog-link:visited";
    case ElementKey::DialogScrollbar: return "dialog-scrollbar";
    case ElementKey::DialogScrollbarHover: return "dialog-scrollbar:hover";
    case ElementKey::DialogScrollbarThumb: return "dialog-scrollbar-thumb";
    case ElementKey::DialogScrollbarThumbHover: return "dialog-scrollbar-thumb:hover";

    case ElementKey::Dock: return "dock";
    case ElementKey::DockDefaults: return "dock-defaults";
    case ElementKey::DockGrip: return "dock-grip";
    case ElementKey::DockGripHover: return "dock-grip:hover";
    case ElementKey::DockGripActive: return "dock-grip:active";
    case ElementKey::DockSeparator: return "dock-separator";
    }
    return {};
}

} // namespace Ui::Res::Key
