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

#include "ui/config.h"
#include "ui/res/type/bound.h"
#include "ui/window/event.h"
#include "ui/window/eventtype.h"
#include "ui/window/ismouseevent.h"

#include <initializer_list>

namespace Ui::Window {

/**
 * @brief Match a mouse event against popup-type connectors (topmost first).
 *
 * Composite mode: popups are offscreen, so match by bounds and rebase the
 * coords into the hit window. Otherwise: match the native source handle.
 * Sets event.isPopupEvent on a hit. Shared by the popup/submenu and the
 * (modal) dialog classification - they differ only in the connector type.
 * @return the hit connector, or nullptr.
 */
template <typename Connector>
Connector * matchPopupTarget(std::initializer_list<Connector *> candidates, Event & event)
{
    const bool isMouse = isMouseEvent(event);

    if (g_config.isCompositing) {
        // MouseLeave has no meaningful coords - handled globally by the caller.
        if (!isMouse || event.type == EventType::MouseLeave) {
            return nullptr;
        }
        for (Connector * candidate : candidates) {
            if (candidate == nullptr) {
                continue;
            }
            const Ui::Res::Type::bound_t & b = candidate->window().bound();
            if (b.contains(event.mouse.x, event.mouse.y)) {
                event.mouse.x -= static_cast<int>(b.x);
                event.mouse.y -= static_cast<int>(b.y);
                event.isPopupEvent = true;
                return candidate;
            }
        }
        return nullptr;
    }

    const auto src = event.sourceWindow;
    for (Connector * candidate : candidates) {
        if (candidate != nullptr && src == candidate->window().nativeHandle()) {
            event.isPopupEvent = true;
            return candidate;
        }
    }
    return nullptr;
}

} // namespace Ui::Window
