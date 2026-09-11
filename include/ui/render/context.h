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
#include "ui/elementid.h"
#include "ui/intent.h"
#include "ui/render/binding.h"
#include "ui/render/elementevent.h"
#include "ui/render/renderscope.h"
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
// UiRenderer/element_event_t). Relocates to ui/context.h (Ui::Context) once the
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
    [[nodiscard]] Ui::result_t mapClick(const element_event_t & click, id_t openMenuId) const
    {
        Ui::result_t result;

        // The two data-dependent types first: their intent comes from res data
        // rather than their type, so no (type, event) binding can express them.
        if (click.type == UiElementType::MenuButton) {
            // With an actionKey it is an action button; without one it is a
            // popup opener, and clicking the open menu closes it.
            const std::string actionKey = m_resManager.actionKeyFor(click.id);
            if (!actionKey.empty()) {
                result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, actionKey, {} });
            } else {
                const Ui::IntentKind kind = (click.id == openMenuId) ? Ui::IntentKind::ClosePopup
                                                                     : Ui::IntentKind::OpenPopup;
                result.intents.push_back({ kind, click.id, {}, {} });
            }
            return result;
        }

        if (click.type == UiElementType::MenuItem) {
            const Ui::Res::Type::menu_t item = m_resManager.findMenuItem(click.id);
            if (!item.items.empty()) {
                return result; // submenu parent: opened on hover, no click intent
            }
            // Clicking a leaf dismisses the menu, then either opens its dialog or
            // emits its action (label is the value parameterized actions consume).
            result.intents.push_back({ Ui::IntentKind::ClosePopup, INVALID_ID, {}, {} });
            if (!item.dialog.title.empty()) {
                result.intents.push_back({ Ui::IntentKind::OpenDialog, click.id, {}, {} });
            } else {
                result.intents.push_back({ Ui::IntentKind::EmitAction, INVALID_ID, item.actionKey, item.label });
            }
            return result;
        }

        // Everything else is declared: the binding names the intent, and only
        // the payload differs per intent kind. A type with no binding for this
        // event yields nothing, which is how an element opts out.
        binding_t binding;
        if (!defaultBinding(click.type, click.event, binding)) {
            return result;
        }

        switch (binding.intent) {
        case Ui::IntentKind::EmitAction: {
            const std::string actionKey = m_resManager.actionKeyFor(click.id);
            if (!actionKey.empty()) {
                result.intents.push_back({ binding.intent, INVALID_ID, actionKey, {} });
            }
            break;
        }
        case Ui::IntentKind::CopyText:
            result.intents.push_back({ binding.intent, INVALID_ID, {}, m_resManager.statusText() });
            break;
        case Ui::IntentKind::ActivateRow:
        case Ui::IntentKind::ToggleRow:
            // Back out of the reserved range, so the host receives the row id
            // it projected rather than an element id it never issued
            result.intents.push_back({ binding.intent, Ui::toDockRowId(click.id), {}, {} });
            break;
        default:
            // id-carrying intents (SwitchTab, CloseTab, OpenPopup, OpenDialog)
            result.intents.push_back({ binding.intent, click.id, {}, {} });
            break;
        }
        return result;
    }

    // The repaint a click needs, for a caller that has already executed the
    // intents. None for a type with no binding, so an unhandled click cannot
    // force a frame.
    [[nodiscard]] static RenderScope scopeFor(const element_event_t & click)
    {
        binding_t binding;
        if (defaultBinding(click.type, click.event, binding)) {
            return binding.scope;
        }
        // The data-dependent types both mutate chrome: a popup opens or closes,
        // or an action runs against the menu that raised it
        if (click.type == UiElementType::MenuButton || click.type == UiElementType::MenuItem) {
            return RenderScope::Chrome;
        }
        return RenderScope::None;
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
