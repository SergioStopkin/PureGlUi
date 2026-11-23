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
#include "ui/animation/easedof.h"
#include "ui/animation/motion.h"
#include "ui/animation/track.h"
#include "ui/animation/transition.h"
#include "ui/color.h"
#include "ui/res/type/bound.h"
#include "ui/res/type/colorpair.h"
#include "ui/type.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <unordered_map>
#include <vector>

namespace Ui::Animation {

// Where each element is drawn, and in which colours, while they glide, by `Key` - an
// element's identity. Layout and the theme stay the truth - hit-testing reads them - and
// drawing asks here. FLIP: a property given a new value glides there from where it was
// drawn, one changed again mid-glide from where it stands, each on its own clock
template <typename Key>
class Animator final {
public:
    // A frame begins at `now`: until the next, every glide is drawn where it stands then, and
    // isRunning says whether one is still on its way
    void beginFrame(Common::Time::point_t now) { m_now = now; }

    // Where `key` is drawn this frame, layout putting it at `target`: there at once the first
    // time and with no transitions, else gliding along `transitions` whenever it moves
    [[nodiscard]] Ui::Res::Type::bound_t boundOf(const Key &                       key,
                                                 const Ui::Res::Type::bound_t &    target,
                                                 const std::vector<transition_t> & transitions)
    {
        if (transitions.empty()) {
            return target;
        }
        motion_t &             motion = seen(key);
        Ui::Res::Type::bound_t drawn  = target;
        for (const AnimatedProperty property :
             { AnimatedProperty::Left, AnimatedProperty::Top, AnimatedProperty::Width, AnimatedProperty::Height }) {
            const auto member = memberOf(property);
            drawn.*member =
                glide(trackOf(motion, property), { target.*member }, transitionFor(transitions, property)).at(0);
        }
        return drawn;
    }

    // The colours `key` is drawn in this frame, the theme giving it `target`: as `boundOf`, a
    // background or a text colour fading through premultiplied RGBA - so from none it fades
    // in rather than through black - and each exactly the theme's once it has arrived
    [[nodiscard]] Ui::Res::Type::color_pair_t colorsOf(const Key &                         key,
                                                       const Ui::Res::Type::color_pair_t & target,
                                                       const std::vector<transition_t> &   transitions)
    {
        if (transitions.empty()) {
            return target;
        }
        motion_t & motion = seen(key);
        const auto fade   = [&](AnimatedProperty property, const Ui::Color & color) {
            track_t &  track = trackOf(motion, property);
            const auto at    = glide(track, channelsOf(color), transitionFor(transitions, property));
            return isAtRest(track, m_now) ? color : colorOf(at);
        };
        return { .fg = fade(AnimatedProperty::Color, target.fg), .bg = fade(AnimatedProperty::Background, target.bg) };
    }

    // `key` drawn at `drawn` by the pointer rather than by layout - a drag; the next boundOf
    // glides from there to wherever layout puts it
    void hold(const Key & key, const Ui::Res::Type::bound_t & drawn)
    {
        motion_t & motion = seen(key);
        for (const AnimatedProperty property :
             { AnimatedProperty::Left, AnimatedProperty::Top, AnimatedProperty::Width, AnimatedProperty::Height }) {
            const std::array<fpx_t, 4> value { drawn.*memberOf(property) };
            trackOf(motion, property) = track_t { .from = value, .target = value, .start = {}, .isKnown = true };
        }
    }

    // `key` in `colors` at once rather than faded to - a change its owner draws as a jump, as the
    // Ctrl+Tab list's highlight moving onto or off a row's close button; as after a hold, the
    // next colorsOf toward other colours fades from them
    void setColors(const Key & key, const Ui::Res::Type::color_pair_t & colors)
    {
        motion_t &                 motion        = seen(key);
        const std::array<fpx_t, 4> fg            = channelsOf(colors.fg);
        const std::array<fpx_t, 4> bg            = channelsOf(colors.bg);
        trackOf(motion, AnimatedProperty::Color) = track_t { .from = fg, .target = fg, .start = {}, .isKnown = true };
        trackOf(motion,
                AnimatedProperty::Background)    = track_t { .from = bg, .target = bg, .start = {}, .isKnown = true };
    }

    // Whether anything still glides at this frame's time, so another frame is due: the frame
    // that lands every glide is drawn
    [[nodiscard]] bool isRunning() const { return m_now < m_until; }

    // Forgets each element not drawn since the last sweep - closed, scrolled off - so one drawn
    // again stands at its place at once; once after every frame that draws them all
    void sweep()
    {
        for (auto it = m_motions.begin(); it != m_motions.end();) {
            if (it->second.isSeen) {
                it->second.isSeen = false;
                ++it;
            } else {
                it = m_motions.erase(it);
            }
        }
    }

private:
    std::unordered_map<Key, motion_t> m_motions;
    Common::Time::point_t             m_now;   // when the frame being drawn began (beginFrame)
    Common::Time::point_t             m_until; // when every glide begun so far has ended

    motion_t & seen(const Key & key)
    {
        motion_t & motion = m_motions[key];
        motion.isSeen     = true;
        return motion;
    }

    // Where a property stands this frame on its way to `target`: there at once the first time,
    // and a target it was not going to starts a glide from where it stands
    std::array<fpx_t, 4> glide(track_t & track, const std::array<fpx_t, 4> & target, const transition_t & transition)
    {
        if (!track.isKnown) {
            track = track_t { .from = target, .target = target, .start = m_now, .isKnown = true };
        } else if (track.target != target) {
            track   = track_t { .from       = standingOf(track, m_now),
                                .target     = target,
                                .start      = m_now,
                                .transition = transition,
                                .isKnown    = true };
            m_until = std::max(m_until, m_now + transition.duration);
        }
        return standingOf(track, m_now);
    }

    [[nodiscard]] static bool isAtRest(const track_t & track, Common::Time::point_t now)
    {
        return track.transition.duration <= Common::Time::span_t::zero()
            || now >= track.start + track.transition.duration;
    }

    // Where a track stands at `now`: its target exactly once arrived, else along its easing
    [[nodiscard]] static std::array<fpx_t, 4> standingOf(const track_t & track, Common::Time::point_t now)
    {
        if (isAtRest(track, now)) {
            return track.target;
        }
        const fpx_t          progress = std::chrono::duration<fpx_t>(now - track.start)
                                      / std::chrono::duration<fpx_t>(track.transition.duration);
        const fpx_t          eased    = easedOf(track.transition.easing, progress);
        std::array<fpx_t, 4> at {};
        for (std::size_t idx = 0; idx < at.size(); ++idx) {
            at.at(idx) = track.from.at(idx) + ((track.target.at(idx) - track.from.at(idx)) * eased);
        }
        return at;
    }

    // A property's transition; one with no duration where `transitions` names none
    [[nodiscard]] static transition_t transitionFor(const std::vector<transition_t> & transitions,
                                                    AnimatedProperty                  property)
    {
        const auto it = std::ranges::find(transitions, property, &transition_t::property);
        return it != transitions.end() ? *it : transition_t { .property = property };
    }

    [[nodiscard]] static track_t & trackOf(motion_t & motion, AnimatedProperty property)
    {
        switch (property) {
        case AnimatedProperty::Left: return motion.left;
        case AnimatedProperty::Top: return motion.top;
        case AnimatedProperty::Width: return motion.width;
        case AnimatedProperty::Height: return motion.height;
        case AnimatedProperty::Background: return motion.background;
        case AnimatedProperty::Color: return motion.color;
        }
        return motion.left;
    }

    [[nodiscard]] static fpx_t Ui::Res::Type::bound_t::* memberOf(AnimatedProperty property)
    {
        switch (property) {
        case AnimatedProperty::Left: return &Ui::Res::Type::bound_t::x;
        case AnimatedProperty::Top: return &Ui::Res::Type::bound_t::y;
        case AnimatedProperty::Width: return &Ui::Res::Type::bound_t::w;
        case AnimatedProperty::Height: return &Ui::Res::Type::bound_t::h;
        case AnimatedProperty::Background:
        case AnimatedProperty::Color: break;
        }
        return &Ui::Res::Type::bound_t::x;
    }

    // A colour as premultiplied RGBA, 0..1; inherited - none of its own - as transparent
    [[nodiscard]] static std::array<fpx_t, 4> channelsOf(const Ui::Color & color)
    {
        if (color.isInherit()) {
            return {};
        }
        const fpx_t alpha = color.a() / 255.0F;
        return { color.r() / 255.0F * alpha, color.g() / 255.0F * alpha, color.b() / 255.0F * alpha, alpha };
    }

    [[nodiscard]] static Ui::Color colorOf(const std::array<fpx_t, 4> & channels)
    {
        const fpx_t alpha = std::clamp(channels.at(3), 0.0F, 1.0F);
        if (alpha <= 0.0F) {
            return Ui::Color::TransparentBlack();
        }
        const auto byteOf = [](fpx_t value) {
            return static_cast<int>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
        };
        return { byteOf(channels.at(0) / alpha),
                 byteOf(channels.at(1) / alpha),
                 byteOf(channels.at(2) / alpha),
                 byteOf(alpha) };
    }
};

} // namespace Ui::Animation
