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

#include "ui/window/connector/dialogconnector.h"
#include "ui/window/connector/menuconnector.h"
#include "ui/window/connector/tabswitcherconnector.h"
#include "ui/window/connector/tooltipconnector.h"
#include "ui/window/popupslot.h"

#include <tuple>

namespace Ui::Window {

// Every popup-type window the WindowManager owns, drawn into the main window in this order
// (forEach): a menu - a top-bar dropdown or a dock row's - and the submenu beside it, the
// modal dialog, the Ctrl+Tab list (what it shows is the shell's, Ui::TabSwitch) and the
// tooltip. Each the part that behaves as it does is handed its own slot
struct alignas(128) popup_slots_t final {
    popup_slot_t<MenuConnector>        menu;
    popup_slot_t<MenuConnector>        submenu;
    popup_slot_t<DialogConnector>      dialog;
    popup_slot_t<TabSwitcherConnector> tabSwitcher;
    popup_slot_t<TooltipConnector>     tooltip;

    // Every slot handed to `fn`, an empty one where none is open, in drawing order: the one
    // list of them all
    template <typename Fn>
    void forEach(const Fn & fn)
    {
        std::apply([&fn](auto &... slot) { (fn(slot), ...); }, std::tie(menu, submenu, dialog, tabSwitcher, tooltip));
    }
};

} // namespace Ui::Window
