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

#include "common/json.h"
#include "nlohmann/json.hpp"
#include "ui/animation/transition.h"
#include "ui/animation/transitionsof.h"
#include "ui/res/key/cssprop.h"
#include "ui/res/key/element.h"
#include "ui/type.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Res::Store {

// Loads and holds res/animation.json: how each element moves, apart from where it stands
// (layout.json) and how it looks (the theme). A block per element, named as in both, its
// `transition` CSS's; an element with none, or no file at all, jumps. One of the logical
// sub-stores ResManager composes
class AnimationStore final {
    static constexpr fpx_t HALF = 0.5F; // the swap a file naming none gets

    std::unordered_map<std::string, std::vector<Ui::Animation::transition_t>> m_transitions; // by block name
    fpx_t m_tabSwapAt = HALF; // workspace-tab's `swap-at`, 0..1

public:
    // The transitions of the block named `block`; none when it has none
    [[nodiscard]] const std::vector<Ui::Animation::transition_t> & transitionsFor(const std::string & block) const
    {
        static const std::vector<Ui::Animation::transition_t> none;
        const auto                                            it = m_transitions.find(block);
        return it != m_transitions.end() ? it->second : none;
    }

    // How far over a neighbour a dragged tab is drawn before they swap: a share of the
    // neighbour's width, 0..1 - 0.5 its leading edge at the neighbour's middle, 1 over it whole
    [[nodiscard]] fpx_t tabSwapAt() const { return m_tabSwapAt; }

    // Reads `file` again in full, so a reload drops a block taken out of it
    void load(const std::string & file)
    {
        m_transitions.clear();
        m_tabSwapAt = HALF;
        nlohmann::json json;
        if (!Common::loadJson(file, json) || !json.is_object()) {
            return;
        }
        const std::string transitionKey = Ui::Res::Key::cssPropKeyName(Ui::Res::Key::CssPropKey::Transition);
        for (auto it = json.begin(); it != json.end(); ++it) {
            auto transitions = Ui::Animation::transitionsOf(Common::Json::sanitizedString(it.value(), transitionKey));
            if (!transitions.empty()) {
                m_transitions.emplace(it.key(), std::move(transitions));
            }
        }
        const auto & tab = Common::Json::object(json,
                                                Ui::Res::Key::elementKeyName(Ui::Res::Key::ElementKey::WorkspaceTab));
        m_tabSwapAt      = std::clamp(
        Common::Json::number(tab, Ui::Res::Key::cssPropKeyName(Ui::Res::Key::CssPropKey::SwapAt), HALF),
        0.0F,
        1.0F);
    }
};

} // namespace Ui::Res::Store
