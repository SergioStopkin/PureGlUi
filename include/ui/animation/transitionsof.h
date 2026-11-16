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

#include "common/time.h"
#include "ui/animation/animatedproperty.h"
#include "ui/animation/easing.h"
#include "ui/animation/transition.h"
#include "ui/convert.h"
#include "ui/res/key/cssprop.h"
#include "ui/res/key/transition.h"
#include "ui/type.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui::Animation {

// The transitions a res `transition` value declares, CSS's spelling: comma-separated parts of
// a property, a duration in `ms` or `s` and an easing, in any order, the property `all` when
// left out and the easing `ease`. A part with no time, or a word it does not know - a delay
// among them - is left out, moving nothing; a property declared twice takes its last part
[[nodiscard]] inline std::vector<transition_t> transitionsOf(std::string_view css)
{
    static constexpr std::array PROPERTIES {
        std::pair { Ui::Res::Key::CssPropKey::Left, AnimatedProperty::Left },
        std::pair { Ui::Res::Key::CssPropKey::Top, AnimatedProperty::Top },
        std::pair { Ui::Res::Key::CssPropKey::Width, AnimatedProperty::Width },
        std::pair { Ui::Res::Key::CssPropKey::Height, AnimatedProperty::Height },
        std::pair { Ui::Res::Key::CssPropKey::Background, AnimatedProperty::Background },
        std::pair { Ui::Res::Key::CssPropKey::Color, AnimatedProperty::Color },
    };
    static constexpr std::array EASINGS {
        std::pair { Ui::Res::Key::TransitionKey::Linear, LINEAR },
        std::pair { Ui::Res::Key::TransitionKey::Ease, EASE },
        std::pair { Ui::Res::Key::TransitionKey::EaseIn, EASE_IN },
        std::pair { Ui::Res::Key::TransitionKey::EaseOut, EASE_OUT },
        std::pair { Ui::Res::Key::TransitionKey::EaseInOut, EASE_IN_OUT },
    };
    const std::string all    = Ui::Res::Key::transitionKeyName(Ui::Res::Key::TransitionKey::All);
    const std::string bezier = Ui::Res::Key::transitionKeyName(Ui::Res::Key::TransitionKey::CubicBezier) + "(";

    // A time, "150ms" or "0.15s"; none for any other word
    const auto spanOf = [](std::string_view word) -> std::optional<Common::Time::span_t> {
        const bool isMs = word.ends_with("ms");
        if (word.empty() || ((word.front() < '0' || word.front() > '9') && word.front() != '.')
            || (!isMs && !word.ends_with("s"))) {
            return std::nullopt;
        }
        const fpx_t number = Ui::Convert::parseCssNumber(std::string(word.substr(0, word.size() - (isMs ? 2 : 1))));
        if (isMs) {
            return std::chrono::duration_cast<Common::Time::span_t>(std::chrono::duration<fpx_t, std::milli>(number));
        }
        return std::chrono::duration_cast<Common::Time::span_t>(std::chrono::duration<fpx_t>(number));
    };

    // A property by its name; none for `all` or any other word
    const auto propertyOf = [](std::string_view word) -> std::optional<AnimatedProperty> {
        for (const auto & entry : PROPERTIES) {
            if (Ui::Res::Key::cssPropKeyName(entry.first) == word) {
                return entry.second;
            }
        }
        return std::nullopt;
    };

    // An easing, a keyword or cubic-bezier(x1, y1, x2, y2) with both x within 0..1
    const auto easingOf = [&bezier](std::string_view word) -> std::optional<easing_t> {
        for (const auto & entry : EASINGS) {
            if (Ui::Res::Key::transitionKeyName(entry.first) == word) {
                return entry.second;
            }
        }
        if (!word.starts_with(bezier) || !word.ends_with(")")) {
            return std::nullopt;
        }
        std::vector<fpx_t> numbers;
        std::string_view   rest = word.substr(bezier.size(), word.size() - bezier.size() - 1);
        for (std::size_t comma = rest.find(','); comma != std::string_view::npos; comma = rest.find(',')) {
            numbers.emplace_back(Ui::Convert::parseCssNumber(std::string(rest.substr(0, comma))));
            rest = rest.substr(comma + 1);
        }
        numbers.emplace_back(Ui::Convert::parseCssNumber(std::string(rest)));
        if (numbers.size() != 4 || numbers.at(0) < 0.0F || numbers.at(0) > 1.0F || numbers.at(2) < 0.0F
            || numbers.at(2) > 1.0F) {
            return std::nullopt;
        }
        return easing_t { .x1 = numbers.at(0), .y1 = numbers.at(1), .x2 = numbers.at(2), .y2 = numbers.at(3) };
    };

    std::vector<transition_t> transitions;
    const auto                addPart = [&](const std::vector<std::string_view> & words) {
        std::optional<AnimatedProperty>     property; // none: all
        bool                                hasProperty = false;
        std::optional<Common::Time::span_t> duration;
        std::optional<easing_t>             easing;
        const auto                          isTaken = [&](std::string_view word) {
            const std::optional<AnimatedProperty> named = propertyOf(word);
            if (!hasProperty && (named.has_value() || word == all)) {
                hasProperty = true;
                property    = named;
                return true;
            }
            if (!duration.has_value()) {
                duration = spanOf(word);
                if (duration.has_value()) {
                    return true;
                }
            }
            if (!easing.has_value()) {
                easing = easingOf(word);
                return easing.has_value();
            }
            return false;
        };
        if (!std::ranges::all_of(words, isTaken) || !duration.has_value()
            || duration.value() <= Common::Time::span_t::zero()) {
            return;
        }
        const bool                 isAll = !property.has_value();
        const AnimatedProperty     only  = property.value_or(AnimatedProperty::Left);
        const Common::Time::span_t span  = duration.value();
        const easing_t             curve = easing.value_or(EASE);
        for (const auto & entry : PROPERTIES) {
            if (isAll || entry.second == only) {
                std::erase_if(transitions, [&entry](const transition_t & transition) {
                    return transition.property == entry.second;
                });
                transitions.emplace_back(transition_t { .property = entry.second, .duration = span, .easing = curve });
            }
        }
    };

    // Words split at spaces and parts at commas, neither inside a cubic-bezier's parentheses
    std::vector<std::string_view> words;
    int                           depth     = 0;
    std::size_t                   wordStart = 0;
    const auto                    endWord   = [&](std::size_t end) {
        if (end > wordStart) {
            words.emplace_back(css.substr(wordStart, end - wordStart));
        }
        wordStart = end + 1;
    };
    for (std::size_t idx = 0; idx < css.size(); ++idx) {
        const char letter = css.at(idx);
        if (letter == '(') {
            ++depth;
        } else if (letter == ')') {
            --depth;
        } else if (depth == 0 && (letter == ' ' || letter == '\t')) {
            endWord(idx);
        } else if (depth == 0 && letter == ',') {
            endWord(idx);
            addPart(words);
            words.clear();
        }
    }
    endWord(css.size());
    addPart(words);
    return transitions;
}

} // namespace Ui::Animation
