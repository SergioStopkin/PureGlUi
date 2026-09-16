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

#include "ui/res/type/menu.h"
#include "ui/res/type/popup.h"
#include "ui/type.h"

#include <vector>

namespace Ui::Res::Store {

// The CSS height a popup needs to show these items: one item height per visible
// item, one separator block per visible separator. The single rule behind a
// top-menu dropdown, a submenu and a dock row's menu, so the three cannot size the
// same list differently.
[[nodiscard]] inline fpx_t menuHeightOf(const std::vector<Ui::Res::Type::menu_t> & items,
                                        const Ui::Res::Type::popup_t &             popup)
{
    int regularCount   = 0;
    int separatorCount = 0;
    for (const Ui::Res::Type::menu_t & item : items) {
        if (!item.visible) {
            continue;
        }
        if (item.separator) {
            ++separatorCount;
        } else {
            ++regularCount;
        }
    }
    return (regularCount * popup.itemHeight)
         + (separatorCount * (popup.separatorHeight + (popup.separatorMarginV * 2)));
}

} // namespace Ui::Res::Store
