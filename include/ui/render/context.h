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

#include "common/noncopyable.h"
#include "ui/intent.h"
#include "ui/render/clickresult.h"
#include "ui/res/resmanager.h"
#include "ui/result.h"
#include "ui/type.h"

#include <string>

namespace Ui::Render {

// The framework's UI facade (intents-out). It interprets main-UI input against
// the resource data and emits intents for the host to execute; it never runs an
// action, opens a window, or mutates a workspace itself.
//
// Staged pureglui-side for now (it reads ResManager + the pureglui
// UiRenderer/click_result_t). Relocates to ui/context.h (Ui::Context) once the
// render layer is fw-resident and ResManager is severed (roadmap step 8).
class Context final : private Common::NonCopyable {
public:
    explicit Context(const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
    {
    }

    // Map a resolved main-UI click to intents. openMenuId is the currently-open
    // top menu, so a MenuButton click toggles open vs close. Execution is left to
    // the shell (which dispatches the resulting intents).
    [[nodiscard]] Ui::result_t mapClick(const click_result_t & click, id_t openMenuId) const
    {
        Ui::result_t result;
        switch (click.type) {
        case UiElementType::MenuButton: {
            const std::string actionKey = m_resManager.actionKeyFor(click.id);
            if (!actionKey.empty()) {
                result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, actionKey, {} });
            } else {
                const Ui::IntentKind kind = (click.id == openMenuId) ? Ui::IntentKind::ClosePopup
                                                                     : Ui::IntentKind::OpenPopup;
                result.intents.push_back({ kind, click.id, {}, {} });
            }
            break;
        }
        case UiElementType::MenuItem: {
            const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(click.id);
            if (!item.items.empty()) {
                break; // submenu parent: opened on hover, no click intent
            }
            // Clicking a leaf dismisses the menu, then either opens its dialog or
            // emits its action (label is the value parameterized actions consume).
            result.intents.push_back({ Ui::IntentKind::ClosePopup, INVALID_ID, {}, {} });
            if (!item.dialog.title.empty()) {
                result.intents.push_back({ Ui::IntentKind::OpenDialog, click.id, {}, {} });
            } else {
                result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, item.actionKey, item.label });
            }
            break;
        }
        case UiElementType::Tab: result.intents.push_back({ Ui::IntentKind::SwitchTab, click.id, {}, {} }); break;
        case UiElementType::TabClose: result.intents.push_back({ Ui::IntentKind::CloseTab, click.id, {}, {} }); break;
        case UiElementType::ToolbarButton: {
            const std::string actionKey = m_resManager.actionKeyFor(click.id);
            if (!actionKey.empty()) {
                result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, actionKey, {} });
            }
            break;
        }
        case UiElementType::Text:
            result.intents.push_back({ Ui::IntentKind::CopyText, INVALID_ID, {}, m_resManager.statusText() });
            break;
        default: break;
        }
        return result;
    }

    // Map a normalized keyboard shortcut to an action intent. The shell handles
    // dialog navigation and shell-only keys before calling this; here we only
    // resolve data-driven shortcuts (shortcuts.json) to EmitAction.
    [[nodiscard]] Ui::result_t mapKey(const std::string & normalizedKey) const
    {
        Ui::result_t result;
        const auto & shortcuts = m_resManager.shortcuts();
        const auto   it        = shortcuts.find(normalizedKey);
        if (it != shortcuts.end() && !it->second.empty()) {
            result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, it->second, {} });
        }
        return result;
    }

private:
    const Ui::Res::ResManager & m_resManager;
};

} // namespace Ui::Render
