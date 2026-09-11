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

#include "ui/color.h"
#include "ui/render/axis.h"
#include "ui/render/thumbmetrics.h"
#include "ui/render/thumbtrack.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <algorithm>

namespace Ui::Render {

// The vertical scrollbar every scrollable surface uses: the dialog and the docks.
// It owns the state (hover, grab) and every decision the "scrollbar" res block
// drives - which width, offset, radius and minimum thumb the current state takes.
//
// It does NOT draw: the dialog paints through Rounded's split-corner path, a dock
// emits ops through UiRenderer. Nor does it own the VALUE, which is why the dialog
// can animate a pixel offset while a dock keeps a row index.
//
// Hit geometry is always the HOVER width, drawn geometry is the current state's -
// otherwise the pointer falls out of the bar the moment its own hover widens it.
class ScrollBar final {
    thumb_metrics_t             m_metrics {};
    thumb_track_t               m_thumb;
    Ui::Res::Type::bound_t      m_drawTrack {};
    const Ui::Res::ResManager & m_resManager;
    fpx_t                       m_viewportHeight {}; // kept for paging, which metrics no longer carries

public:
    explicit ScrollBar(const Ui::Res::ResManager & resManager)
        : m_resManager(resManager)
    {
    }

    // The strip the bar sits at the right edge of, and how much content there is
    // against how much of it shows. Refreshed per layout pass; every accessor
    // below reads what this stored.
    void setGeometry(const Ui::Res::Type::bound_t & area, fpx_t contentHeight, fpx_t viewportHeight)
    {
        const auto & layout    = m_resManager.layout();
        const bool   isHovered = m_thumb.isHoveredTrack;

        const Ui::Res::Type::bound_t track    = trackIn(area, layout.scrollbarHoverW, layout.scrollbarHoverRight);
        const fpx_t                  minThumb = isHovered ? layout.scrollbarHoverMinThumb : layout.scrollbarMinThumb;

        m_viewportHeight = viewportHeight;
        m_metrics        = { track,
                             thumbLengthFor(track.h, contentHeight, viewportHeight, minThumb),
                             hiddenOf(contentHeight, viewportHeight),
                             Axis::Vertical };
        m_drawTrack      = isHovered ? track : trackIn(area, layout.scrollbarW, layout.scrollbarRight);
    }

    [[nodiscard]] const thumb_metrics_t &        metrics() const { return m_metrics; }
    [[nodiscard]] const Ui::Res::Type::bound_t & track() const { return m_drawTrack; }
    [[nodiscard]] bool                           isDragging() const { return m_thumb.isDragging; }
    [[nodiscard]] bool                           isHovered() const { return m_thumb.isHoveredTrack; }

    // Drawn thumb: its span comes from the hit metrics, its column from the drawn
    // track, so a widening bar keeps the thumb it was showing
    [[nodiscard]] Ui::Res::Type::bound_t thumb(fpx_t value) const
    {
        const Ui::Res::Type::bound_t span = thumb_track_t::thumbBound(m_metrics, value);
        return { m_drawTrack.x, span.y, m_drawTrack.w, span.h };
    }

    [[nodiscard]] Ui::Res::Type::border_t radius() const
    {
        const auto & layout = m_resManager.layout();
        return m_thumb.isHoveredTrack ? layout.scrollbarHoverBorder : layout.scrollbarBorder;
    }

    [[nodiscard]] const Ui::Color & trackColor() const { return m_resManager.theme().scrollbarTrack; }

    [[nodiscard]] const Ui::Color & thumbColor() const
    {
        const auto & theme = m_resManager.theme();
        return m_thumb.isHoveredThumb ? theme.scrollbarThumbHover : theme.scrollbarThumb;
    }

    // ---- Gestures. Each returns whether the caller has something to repaint or
    // a value to take; the value itself stays with the caller.

    bool onMouseMove(fpx_t cssX, fpx_t cssY, fpx_t value) { return m_thumb.updateHover(m_metrics, value, cssX, cssY); }

    [[nodiscard]] bool contains(fpx_t cssX, fpx_t cssY) const { return m_metrics.track.contains(cssX, cssY); }

    // On the thumb this grabs and the caller keeps the pointer; anywhere else on
    // the track it pages by one screenful and the gesture is over - isDragging()
    // is what tells the caller which happened.
    //
    // `value` is what is on screen: the thumb the user aimed at. `pageFrom` is
    // what a page steps from, which for an animated caller is its target rather
    // than the frame still catching up to it.
    fpx_t press(fpx_t cssY, fpx_t value, fpx_t pageFrom)
    {
        if (thumb_track_t::isOnThumb(m_metrics, value, cssY)) {
            m_thumb.begin(m_metrics, cssY, value);
            return value;
        }
        return pagedValue(pageFrom, cssY < thumb_track_t::thumbBound(m_metrics, value).y);
    }

    [[nodiscard]] fpx_t valueAt(fpx_t cssY) const { return m_thumb.valueAt(m_metrics, cssY); }

    void release() { m_thumb.isDragging = false; }

    void clear() { m_thumb.clear(); }

private:
    // Right edge of `area`, pushed off it by `right` - which is authored negative
    // in res, so the bar overhangs the content rather than eating into it
    [[nodiscard]] static Ui::Res::Type::bound_t trackIn(const Ui::Res::Type::bound_t & area, fpx_t width, fpx_t right)
    {
        return { area.x + area.w - right - width, area.y, width, area.h };
    }

    // Extent that is off screen, which is the value's upper bound
    [[nodiscard]] static fpx_t hiddenOf(fpx_t contentHeight, fpx_t viewportHeight)
    {
        const fpx_t hidden = contentHeight - viewportHeight;
        return (hidden > 0.0F) ? hidden : 0.0F;
    }

    // Shrinks with the fraction on screen, floored so it stays grabbable. The
    // floor is itself capped by the track: a track shorter than the minimum would
    // otherwise hand std::clamp a lo above its hi, which is undefined
    [[nodiscard]] static fpx_t
    thumbLengthFor(fpx_t trackLength, fpx_t contentHeight, fpx_t viewportHeight, fpx_t minThumb)
    {
        if (contentHeight <= 0.0F) {
            return trackLength;
        }
        const fpx_t proportional = trackLength * (viewportHeight / contentHeight);
        return std::clamp(proportional, std::min(minThumb, trackLength), trackLength);
    }

    // One screenful. The direction is the caller's to decide, because a caller
    // that animates pages from its target while reading the side from the thumb
    // the user can actually see
    [[nodiscard]] fpx_t pagedValue(fpx_t value, bool isUp) const
    {
        const fpx_t page = isUp ? -m_viewportHeight : m_viewportHeight;
        return std::clamp(value + page, 0.0F, m_metrics.maxValue);
    }
};

} // namespace Ui::Render
