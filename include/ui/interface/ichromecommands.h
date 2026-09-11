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

#include "ui/intent.h"
#include "ui/type.h"

#include <string>

namespace Ui {

// The window/chrome commands the intent-execution seam issues. Ui::Shell
// implements it against WindowManager + the action registry + host tab hooks;
// a test implements a recording double. routeIntent() below is the only caller,
// so this interface is the exact vocabulary of "what an intent does".
class IChromeCommands {
public:
    virtual ~IChromeCommands() = default;

    // EmitAction: dispatch a data-driven action (arg is the item label/value for
    // parameterized actions, "" otherwise).
    virtual void emitAction(const std::string & actionKey, const std::string & arg) = 0;

    // Popup lifecycle. isPopupOpen() gates the ClosePopup path (a stray close with
    // no open popup is a no-op, as in the pre-seam switch).
    [[nodiscard]] virtual bool isPopupOpen() const    = 0;
    virtual void               openPopup(id_t menuId) = 0;
    virtual void               closePopup()           = 0;

    // Open the modal dialog carried by a menu item (resolved by item id).
    virtual void openDialog(id_t itemId) = 0;

    // Tab intents (host-domain reactions behind the shell's tab hooks).
    virtual void switchTab(id_t tabId) = 0;
    virtual void closeTab(id_t tabId)  = 0;

    // Copy text to the clipboard (status-bar text click).
    virtual void copyText(const std::string & text) = 0;

    // Dock row intents (host-projected content; rowId is the host's own id).
    virtual void activateRow(id_t rowId) = 0;
    virtual void toggleRow(id_t rowId)   = 0;
};

// Route one intent to its chrome command. The intents-in counterpart to
// Ui::Render::Context (intents-out): a pure switch over IntentKind touching only
// the interface, so it is identical in the app and in tests. Kept free of state
// so both sides share one copy.
inline void routeIntent(const intent_t & intent, IChromeCommands & chrome)
{
    switch (intent.kind) {
    case IntentKind::EmitAction: chrome.emitAction(intent.actionKey, intent.arg); break;
    case IntentKind::OpenPopup: chrome.openPopup(intent.id); break;
    case IntentKind::ClosePopup:
        if (chrome.isPopupOpen()) {
            chrome.closePopup();
        }
        break;
    case IntentKind::OpenSubmenu: break; // submenus open on hover; no click path
    case IntentKind::OpenDialog: chrome.openDialog(intent.id); break;
    case IntentKind::SwitchTab: chrome.switchTab(intent.id); break;
    case IntentKind::CloseTab: chrome.closeTab(intent.id); break;
    case IntentKind::CopyText:
        if (!intent.arg.empty()) {
            chrome.copyText(intent.arg);
        }
        break;
    case IntentKind::ActivateRow: chrome.activateRow(intent.id); break;
    case IntentKind::ToggleRow: chrome.toggleRow(intent.id); break;
    }
}

} // namespace Ui
