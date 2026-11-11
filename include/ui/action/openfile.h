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

namespace Ui::Action {

// "OpenFile": the native open dialog, its files reaching openPickedFiles once it closes - on
// Linux from a later pass of the loop, which runs on while it is open. Title + filters are
// data-driven (res app.json); empty filters => any file
template <typename Host>
void openFile(Host & host, const std::string & /*arg*/)
{
    host.closePopupMenu();
    host.fileDialog().open(host.windowManager().mainWindow(),
                           host.resManager().lastOpenDir(),
                           host.resManager().openFileTitle(),
                           host.resManager().openFileFilters());
}

} // namespace Ui::Action
