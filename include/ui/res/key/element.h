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
    Host, // host block - values the framework parses but never interprets

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

    Window,                 // window
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

    Dialog,              // dialog
    DialogTitle,         // dialog-title
    DialogText,          // dialog-text
    DialogIcon,          // dialog-icon
    DialogClose,         // dialog-close
    DialogCloseHover,    // dialog-close:hover
    DialogCloseActive,   // dialog-close:active
    DialogButton,        // dialog-button
    DialogButtonHover,   // dialog-button:hover
    DialogButtonActive,  // dialog-button:active
    DialogButtonPrimary, // dialog-button:primary
    DialogLink,          // dialog-link
    DialogLinkVisited,   // dialog-link:visited
    Scrollbar,           // scrollbar - shared by the dialog and the docks
    ScrollbarHover,      // scrollbar:hover
    ScrollbarThumb,      // scrollbar-thumb
    ScrollbarThumbHover, // scrollbar-thumb:hover

    Dock,           // dock
    DockDefaults,   // dock-defaults
    DockGrip,       // dock-grip
    DockGripHover,  // dock-grip:hover
    DockGripActive, // dock-grip:active
    // Slider parts, named the way scrollbar-thumb is. The state suffix is a
    // POINTER state here: the filled portion is not a state of anything, so it
    // shares the thumb's colour - both mark the value, and a slider that coloured
    // them apart would read as two different indicators.
    DockSliderTrack,      // dock-slider-track
    DockSliderThumb,      // dock-slider-thumb
    DockSliderThumbHover, // dock-slider-thumb:hover
    DockSeparator,        // dock-separator

    Count, // enumerator total, never a selector - what an exhaustive check counts against
};

[[nodiscard]] inline std::string elementKeyName(ElementKey key)
{
    switch (key) {
    case ElementKey::Root: return ":root";
    case ElementKey::Host: return "host";

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

    case ElementKey::Window: return "window";
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
    case ElementKey::Scrollbar: return "scrollbar";
    case ElementKey::ScrollbarHover: return "scrollbar:hover";
    case ElementKey::ScrollbarThumb: return "scrollbar-thumb";
    case ElementKey::ScrollbarThumbHover: return "scrollbar-thumb:hover";

    case ElementKey::Dock: return "dock";
    case ElementKey::DockDefaults: return "dock-defaults";
    case ElementKey::DockGrip: return "dock-grip";
    case ElementKey::DockGripHover: return "dock-grip:hover";
    case ElementKey::DockGripActive: return "dock-grip:active";
    case ElementKey::DockSliderTrack: return "dock-slider-track";
    case ElementKey::DockSliderThumb: return "dock-slider-thumb";
    case ElementKey::DockSliderThumbHover: return "dock-slider-thumb:hover";
    case ElementKey::DockSeparator: return "dock-separator";

    // Falls through to the same empty string an out-of-range value gets, and
    // still costs a case, so -Wswitch keeps catching a real key with no spelling
    case ElementKey::Count: break;
    }
    return {};
}

} // namespace Ui::Res::Key
