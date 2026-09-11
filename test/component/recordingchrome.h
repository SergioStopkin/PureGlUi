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

#include "ui/interface/ichromecommands.h"
#include "ui/type.h"

#include <string>
#include <vector>

namespace PureGlUi {

// Records the command sequence Ui::routeIntent issues, as readable strings for
// order-sensitive asserts. Tracks the open-menu id so isPopupOpen() gates the
// ClosePopup path exactly as Shell's m_openMenuId does. Shared by the
// chrome-lifecycle suites.
class RecordingChrome final : public Ui::IChromeCommands {
public:
    std::vector<std::string> log;
    Ui::id_t                 openMenuId = Ui::INVALID_ID;

    void emitAction(const std::string & actionKey, const std::string & arg) override
    {
        log.emplace_back("emitAction(" + actionKey + "," + arg + ")");
    }
    [[nodiscard]] bool isPopupOpen() const override { return openMenuId != Ui::INVALID_ID; }
    void               openPopup(Ui::id_t menuId) override
    {
        openMenuId = menuId;
        log.emplace_back("openPopup(" + std::to_string(menuId) + ")");
    }
    void closePopup() override
    {
        openMenuId = Ui::INVALID_ID;
        log.emplace_back("closePopup");
    }
    void openDialog(Ui::id_t itemId) override { log.emplace_back("openDialog(" + std::to_string(itemId) + ")"); }
    void switchTab(Ui::id_t tabId) override { log.emplace_back("switchTab(" + std::to_string(tabId) + ")"); }
    void closeTab(Ui::id_t tabId) override { log.emplace_back("closeTab(" + std::to_string(tabId) + ")"); }
    void copyText(const std::string & text) override { log.emplace_back("copyText(" + text + ")"); }
    void activateRow(Ui::id_t rowId) override { log.emplace_back("activateRow(" + std::to_string(rowId) + ")"); }
    void toggleRow(Ui::id_t rowId) override { log.emplace_back("toggleRow(" + std::to_string(rowId) + ")"); }
};

} // namespace PureGlUi
