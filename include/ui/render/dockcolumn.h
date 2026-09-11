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
#include "ui/gl/svgrenderer.h"
#include "ui/render/dragtrack.h"
#include "ui/render/scrollbar.h"
#include "ui/render/slider.h"
#include "ui/render/thumbmetrics.h"
#include "ui/render/thumbtrack.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/row.h"
#include "ui/res/dock/state.h"
#include "ui/res/key/iconrole.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Ui::Render {

constexpr bool DOCK_DEBUG = false;

// One collapsible dock column anchored to the left or right edge of the
// viewport. Owns the live state (current width + expanded flag) for one
// dock; reads/writes that state through ResManager so session persistence
// is transparent.
//
// Also owns everything drawn inside it - grip, host rows, scrollbar, sliders:
// their ops, their hit elements and their gestures. WindowManager only decides
// which dock holds the pointer
class DockColumn final : private Common::NonCopyable {
public:
    DockColumn(id_t id, const Ui::Res::Dock::dock_config_t & cfg, const Ui::Res::ResManager & resManager)
        : m_id(id)
        , m_config(cfg)
        , m_resManager(resManager)
        , m_state(resManager.dockState(cfg.name))
    {
        // First-ever expand needs a target size. Session may carry memoryX=0
        // for a brand-new dock (no prior release), so seed from config.
        if (m_state.memoryX <= 0.0F) {
            m_state.memoryX = m_config.defaultWidth;
        }
    }

    ~DockColumn() = default;

    [[nodiscard]] id_t                      id() const { return m_id; }
    [[nodiscard]] const std::string &       name() const { return m_config.name; }
    [[nodiscard]] Ui::Res::Dock::DockAnchor anchor() const { return m_config.anchor; }
    [[nodiscard]] int                       order() const { return m_config.order; }

    // Width occupied right now: content + grip, where content can be 0
    // (collapsed). Drives the viewport reflow in WindowManager. While a
    // drag is in flight, the transient width takes precedence so the
    // preview updates without touching session-persisted state.
    [[nodiscard]] fpx_t currentWidth() const
    {
        const fpx_t grip = m_resManager.layout().dockDefaults.gripWidth;
        return (m_isDragging ? m_transientWidth : m_state.width) + grip;
    }

    [[nodiscard]] bool isDragging() const { return m_isDragging; }
    [[nodiscard]] bool isHoveredGrip() const { return m_isHoveredGrip; }

    // Cap the transient drag width to a dynamic ceiling (typically computed
    // from current viewport availability so the dock can't push content surface below
    // zero). No-op when not dragging. Caller responsibility - DockColumn
    // doesn't know viewport state on its own.
    void clampTransientWidth(fpx_t maxExpanded)
    {
        if (m_isDragging && m_transientWidth > maxExpanded) {
            m_transientWidth = maxExpanded < 0.0F ? 0.0F : maxExpanded;
        }
    }

    // Cap the committed (session-persisted) width to a dynamic ceiling.
    // Called by WindowManager after a non-drag state change (e.g. double-
    // click restore) so the freshly-revealed dock can't push other docks
    // off the workspace.
    //
    // memoryX is clamped too. There is no auto-expand-on-viewport-grow
    // path, so keeping a memoryX larger than maxExpanded just persists
    // a stale "restore size" that the next double-click would re-clamp
    // anyway. Capping it keeps session.json honest (no memoryX > width
    // when expanded) and avoids surprising the user with a restore that
    // is silently shrunk on first interaction.
    void clampStateWidth(fpx_t maxExpanded)
    {
        const fpx_t clamped = maxExpanded < 0.0F ? 0.0F : maxExpanded;
        bool        dirty   = false;
        if (m_state.width > clamped) {
            m_state.width = clamped;
            dirty         = true;
        }
        if (m_state.memoryX > clamped) {
            m_state.memoryX = clamped;
            dirty           = true;
        }
        if (dirty) {
            m_resManager.setDockState(m_config.name, m_state);
        }
    }

    // Mouse routing (CSS coords). Returns whether the dock's visual state
    // changed - WindowManager uses this to decide whether to request a re-
    // render. None of these consume the event for the caller; capture
    // decisions live one layer up so we can guarantee mid-drag exclusivity
    // independent of which dock the cursor happens to be hovering.
    [[nodiscard]] bool isOverGrip(fpx_t cssX, fpx_t cssY) const { return grip().contains(cssX, cssY); }

    bool onMouseMove(fpx_t cssX, fpx_t cssY)
    {
        if (m_isDragging) {
            // Positive delta = dock grows. Each anchor grips the viewport-
            // facing edge of its dock, so the gesture that grows the dock
            // mirrors per side: cursor moves right grows a left dock,
            // cursor moves left grows a right dock. Floor at 0 (just the
            // grip remains visible); ceiling is enforced by WindowManager
            // via clampTransientWidth() based on viewport space.
            fpx_t w = m_grip.valueAt(cssX);
            if (w < 0.0F) {
                w = 0.0F;
            }
            const fpx_t widthDelta = w - m_grip.valueAtGrab;
            std::cout << "[DockColumn::drag] name=\"" << m_config.name
                      << "\" anchor=" << (m_config.anchor == Ui::Res::Dock::DockAnchor::Left ? "L" : "R")
                      << " cssX=" << cssX << " startCursor=" << m_grip.grabPos << " widthDelta=" << widthDelta
                      << " newW=" << w << std::endl;
            const bool changed = (m_transientWidth != w);
            m_transientWidth   = w;
            return changed;
        }
        // Both cues are tracked here rather than read off element state: the dock
        // draws itself from the extra-ops hook, where no element is in hand. The
        // scrollbar half is thumb_track_t's own, the same call the dialog makes.
        const bool nowGrip     = isOverGrip(cssX, cssY);
        const bool gripChanged = (nowGrip != m_isHoveredGrip);
        m_isHoveredGrip        = nowGrip;

        // Third cue, same shape as the two above: which thumb the pointer is on,
        // resolved once here so the draw does not hit-test a second time
        const id_t nowThumb     = hotSliderRow(cssX, cssY);
        const bool thumbChanged = (nowThumb != m_hotSliderRow);
        m_hotSliderRow          = nowThumb;

        if (!isScrollable()) {
            m_scrollBar.clear();
            return gripChanged || thumbChanged;
        }
        refreshScrollBar();
        return m_scrollBar.onMouseMove(cssX, cssY, scrollValue()) || gripChanged || thumbChanged;
    }

    // Start a gesture on this dock's grip. The caller has already established
    // that the press landed there, by hit-testing the DockGrip element - so
    // this deliberately does NOT re-test isOverGrip(). The element bound is
    // captured when the layout is built while grip() is computed live, and the
    // two disagreeing would silently swallow the press.
    //
    // Whether a drag actually began is isDragging(): clickCount >= 2 is the
    // toggle gesture (collapse <-> restore memoryX), resolved inline and
    // starting no drag, so the release cannot commit a tiny width.
    void beginGripGesture(fpx_t cssX, int clickCount)
    {
        if (clickCount >= 2) {
            applyDoubleClickToggle();
            return;
        }
        m_isDragging = true;
        // Start from the live committed width (0 when collapsed). This is
        // what gives "smooth from 0" - the drag composes cursor delta onto
        // whatever the dock is showing right now, not onto a remembered
        // restore size.
        //
        // Each anchor grips the viewport-facing edge of its dock, so the
        // gesture that grows the dock mirrors per side: the scale is -1 on the
        // right, where moving left grows it.
        const fpx_t unitsPerPixel = (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) ? 1.0F : -1.0F;
        m_grip.begin(cssX, m_state.width, unitsPerPixel);
        m_transientWidth = m_state.width;
    }

    // Resolve a press-release on the grip. Tiny movement is a no-op single
    // click (toggling lives on double-click). A real drag commits the
    // transient width into m_state.width; if it's non-zero we also refresh
    // memoryX so a future double-click restores what the user just had on
    // screen. Drag-to-zero leaves memoryX alone.
    void onMouseUp(fpx_t cssX)
    {
        if (!m_isDragging) {
            return;
        }
        const auto & defaults = m_resManager.layout().dockDefaults;
        m_isDragging          = false;

        if (m_grip.travelFrom(cssX) < defaults.clickThreshold) {
            return;
        }

        m_state.width = m_transientWidth;
        if (m_transientWidth > 0.0F) {
            m_state.memoryX = m_transientWidth;
        }
        m_resManager.setDockState(m_config.name, m_state);
    }

    // Whole rows, never pixels: GlRender keeps GL_SCISSOR_TEST off, so nothing
    // clips a dock and a partial row at either end would spill outside the
    // content area. Speed comes from the popup scroll tunables, so a dock and a
    // dialog answer the wheel alike
    bool onScroll(fpx_t deltaY)
    {
        const fpx_t rowH = rowHeight();
        if (deltaY == 0.0F || rowH <= 0.0F) {
            return false;
        }
        const auto & input  = m_resManager.input();
        const fpx_t  pixels = deltaY * (input.scrollNatural ? 1.0F : -1.0F) * input.scrollSpeed;

        // A notch always moves at least one row, however coarse the row is
        int rows = static_cast<int>(pixels / rowH);
        if (rows == 0) {
            rows = (pixels > 0.0F) ? 1 : -1;
        }
        return setFirstRow(static_cast<int>(m_firstRow) + rows);
    }

    // A collapsed dock has no content area, and a scrollbar hung off a zero-width
    // one lands outside it - on top of the grip
    [[nodiscard]] bool isScrollable() const { return content().w > 0.0F && maxFirstRow() > 0; }

    // Press on the scrollbar - grab or page, decided by ScrollBar; isDragging()
    // is what tells the caller whether to keep the pointer
    void beginScrollGesture(fpx_t cssY)
    {
        // Nothing to scroll means no bar was drawn and none was hit-tested, but a
        // caller must not have to know that: without this the thumb fills the
        // track, so any press would "grab" it and hold the pointer for nothing
        if (!isScrollable()) {
            return;
        }
        refreshScrollBar();
        const fpx_t value = scrollValue();
        setScrollValue(m_scrollBar.press(cssY, value, value));
    }

    [[nodiscard]] bool isDraggingScroll() const { return m_scrollBar.isDragging(); }

    bool onScrollDrag(fpx_t cssY) { return setScrollValue(m_scrollBar.valueAt(cssY)); }

    void endScrollDrag() { m_scrollBar.release(); }

    // A row element carries the row and not the dock it came from, so this is
    // how a caller finds WHICH dock owns a slider it pressed
    [[nodiscard]] bool hasSliderRow(id_t rowId) const { return sliderIndex(rowId).has_value(); }

    // Pressing the thumb grabs it where it was taken hold of; pressing bare track
    // jumps the thumb to sit centred under the pointer. Both then drag through the
    // same grab, so the thumb stays under the pointer either way.
    // The jumped ratio for the host, empty when the thumb was grabbed in place
    [[nodiscard]] std::optional<fpx_t> beginSliderGesture(id_t rowId, fpx_t cssX)
    {
        const std::optional<std::size_t> index = sliderIndex(rowId);
        if (!index) {
            return std::nullopt;
        }
        m_sliderMetrics   = sliderMetrics(sliderBound(rowsRect(), *index), sliderThumbLength());
        const fpx_t ratio = m_rows[*index].ratio;
        if (thumb_track_t::isOnThumb(m_sliderMetrics, ratio, cssX)) {
            m_sliderDrag.begin(m_sliderMetrics, cssX, ratio);
            return std::nullopt;
        }
        const fpx_t jumped = thumb_track_t::centredValue(m_sliderMetrics, cssX);
        m_sliderDrag.begin(m_sliderMetrics, cssX, jumped);
        return jumped;
    }

    // A slider commits on every move, so each ratio goes straight to the host
    [[nodiscard]] fpx_t onSliderDrag(fpx_t cssX) const { return m_sliderDrag.valueAt(m_sliderMetrics, cssX); }

    void endSliderDrag() { m_sliderDrag.clear(); }

    void onMouseLeave()
    {
        if (m_isDragging) {
            // Cursor left the window mid-drag. If the user dragged a meaningful
            // distance (e.g. all the way to zero, or far enough to expand the
            // dock), treat the leave as an implicit release at the current
            // transient width - mirrors the commit branch of onMouseUp using
            // width delta as a stand-in for the missing cursor position.
            // Below clickThreshold collapses to the no-op case (matches
            // onMouseUp's click-vs-drag gate).
            const fpx_t  widthDelta = m_transientWidth - m_grip.valueAtGrab;
            const fpx_t  absDelta   = widthDelta < 0.0F ? -widthDelta : widthDelta;
            const auto & defaults   = m_resManager.layout().dockDefaults;
            m_isDragging            = false;
            if (absDelta >= defaults.clickThreshold) {
                m_state.width = m_transientWidth;
                if (m_transientWidth > 0.0F) {
                    m_state.memoryX = m_transientWidth;
                }
                m_resManager.setDockState(m_config.name, m_state);
            }
        }
        m_isHoveredGrip = false;
        m_hotSliderRow  = INVALID_ID;
        m_scrollBar.clear();
        m_sliderDrag.clear();
    }

    // Position the dock. innerEdgeX is the CSS x-coordinate of the viewport-
    // facing edge: for Left anchor it's the right edge of the dock; for
    // Right anchor it's the left edge. y/h are the CSS vertical span. All
    // values stay in CSS because UiRenderer's draw ops (BGOp / ImageOp) are
    // submitted in CSS - endFrame scales by g_scale on the way to GL.
    void setLayout(fpx_t y, fpx_t h, fpx_t innerEdgeX)
    {
        const fpx_t w = currentWidth();
        if (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) {
            m_outer = { innerEdgeX - w, y, w, h };
        } else {
            m_outer = { innerEdgeX, y, w, h };
        }
        // A dock that just grew shows more rows, so the offset it was holding can
        // now be past the end
        m_firstRow = std::min(m_firstRow, maxFirstRow());
    }

    [[nodiscard]] Ui::Res::Type::bound_t outer() const { return m_outer; }

    // Content area = outer minus the grip strip on the viewport-facing edge.
    // Empty (zero width) when the dock is collapsed - currentWidth() == grip
    // width in that state, so outer.w - gripW degenerates to 0.
    [[nodiscard]] Ui::Res::Type::bound_t content() const
    {
        const fpx_t gripW    = m_resManager.layout().dockDefaults.gripWidth;
        const fpx_t contentW = m_outer.w > gripW ? m_outer.w - gripW : 0.0F;
        if (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) {
            return { m_outer.x, m_outer.y, contentW, m_outer.h };
        }
        return { m_outer.x + gripW, m_outer.y, contentW, m_outer.h };
    }

    // Resize/collapse grip on the viewport-facing edge.
    [[nodiscard]] Ui::Res::Type::bound_t grip() const
    {
        const fpx_t gripW = m_resManager.layout().dockDefaults.gripWidth;
        if (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) {
            return { m_outer.x + m_outer.w - gripW, m_outer.y, gripW, m_outer.h };
        }
        return { m_outer.x, m_outer.y, gripW, m_outer.h };
    }

    // Emit ops for: content area background (only when expanded), grip strip
    // background, and the 3-dot grip glyph (22EE) centered in the strip.
    // Called from UiRenderer's extra-ops hook installed by WindowManager.
    void render(UiRenderer & out) const
    {
        const auto &                 theme       = m_resManager.theme();
        const Ui::Res::Type::bound_t contentRect = content();
        const Ui::Res::Type::bound_t gripRect    = grip();
        if constexpr (DOCK_DEBUG) {
            std::cout << "[DockColumn::render] name=\"" << m_config.name
                      << "\" anchor=" << (m_config.anchor == Ui::Res::Dock::DockAnchor::Left ? "L" : "R") << " outer=("
                      << m_outer.x << "," << m_outer.y << "," << m_outer.w << "," << m_outer.h << ") content=("
                      << contentRect.x << "," << contentRect.y << "," << contentRect.w << "," << contentRect.h
                      << ") grip=(" << gripRect.x << "," << gripRect.y << "," << gripRect.w << "," << gripRect.h << ")"
                      << std::endl;
        }
        // Docks sit inside the workspace area, so its bg is what the rounded
        // edges blend against for AA. Theme blocks are passed in their
        // natural {fg=text/icon, bg=fill} form; appendBg does the SDF remap.
        const Ui::Color & parentBg = theme.workspace.bg;
        if (contentRect.w > 0.0F) {
            out.appendBg(contentRect, theme.dock.background, parentBg);
        }
        const Ui::Res::Type::color_pair_t & gripColors = m_isDragging    ? theme.dock.gripActive
                                                       : m_isHoveredGrip ? theme.dock.gripHover
                                                                         : theme.dock.grip;
        // Round only the corners on the viewport-facing edge so the seam
        // with the dock content stays sharp. Left anchor -> grip on the
        // right side of outer -> top/bottom-right are the viewport-facing
        // corners; mirror for right anchor
        const fpx_t                   r          = m_resManager.layout().dockDefaults.gripRadius;
        const Ui::Res::Type::border_t gripRadius = (m_config.anchor == Ui::Res::Dock::DockAnchor::Left)
                                                 ? Ui::Res::Type::border_t { 0, r, r, 0 }
                                                 : Ui::Res::Type::border_t { r, 0, 0, r };
        out.appendBg(gripRect, gripColors, parentBg, gripRadius);

        // Glyph width matches the grip strip (the configurable dock-edge
        // width from layout.json). Height comes from the SVG's intrinsic
        // aspect ratio looked up at Ui::Gl::SvgRenderer's cache - the file may be
        // swapped via --dock-grip-icon without code edits. WindowManager
        // preloads the glyph at startup, so the lookup is hot by the first
        // render; if it ever misses (e.g. icon stripped from layout.json),
        // we fall back to a square so the strip still gets a visible mark.
        const std::string & iconName = m_resManager.layout().dockDefaults.gripIcon;
        const std::string   svgPath  = m_resManager.resPath().icon(iconName);
        float               svgW     = 1.0F;
        float               svgH     = 1.0F;
        Ui::Gl::SvgRenderer::contentSizeIfCached(Ui::Gl::SvgRenderer::loadFilledFromFile(svgPath), svgW, svgH);

        const fpx_t                  glyphW = gripRect.w;
        const fpx_t                  glyphH = (svgW > 0.0F) ? glyphW * (svgH / svgW) : glyphW;
        const fpx_t                  glyphY = gripRect.y + (gripRect.h - glyphH) / 2.0F;
        const Ui::Res::Type::bound_t glyph  = { gripRect.x, glyphY, glyphW, glyphH };
        if constexpr (DOCK_DEBUG) {
            std::cout << "[DockColumn::render] glyph=(" << glyph.x << "," << glyph.y << "," << glyph.w << "," << glyph.h
                      << ") svgIntrinsic=(" << svgW << "x" << svgH << ") tint=" << gripColors.fg.toHex() << std::endl;
        }
        // Solid: the grip dots are a handle mark, not line art
        out.appendImage(glyph, svgPath, gripColors.fg, true);

        renderRows(out, rowsRect());
        renderScrollbar(out);
    }

    // Host-projected content. The framework renders and hit-tests these and
    // never learns what they mean - the same contract as Ui::TabBar.
    void setRows(std::vector<Ui::Res::Dock::row_t> rows)
    {
        m_rows = std::move(rows);
        // Collapsing a tree node shrinks the list under a held offset
        m_firstRow = std::min(m_firstRow, maxFirstRow());
    }

    void resetScroll() { m_firstRow = 0; }

    // Top row of the scroll window - what a wheel, a page or a thumb drag moves
    [[nodiscard]] std::size_t firstRow() const { return m_firstRow; }

    [[nodiscard]] const std::vector<Ui::Res::Dock::row_t> & rows() const { return m_rows; }

    // Contribute one element per visible row - plus an expander on top of the
    // rows that have children - so dock content is hit-tested, hovered and
    // bound by the same pipeline as every other element. The element id is the
    // host's own row id, carried through to the intent untouched.
    //
    // Expander after its row so insertion order puts it on top: the same
    // overlap TabClose relies on over Tab.
    void appendElements(UiLayout & layout) const
    {
        appendRowElements(layout);

        // Over the rows it sits beside, for the same reason the expander is over
        // its row: whichever comes later wins the press
        if (isScrollable()) {
            refreshScrollBar();
            layout.addElement(UiElementType::DockScrollbar, m_scrollBar.metrics().track, toDockScrollElementId(m_id));
        }

        // Grip last, so reverse hit-testing reaches it first and a row along
        // the dock edge can never swallow a press meant to resize. Outside
        // appendRowElements because a collapsed dock has no rows at all, and
        // the grip is then the only way to get it back.
        layout.addElement(UiElementType::DockGrip, grip(), toDockGripElementId(m_id));
    }

private:
    void appendRowElements(UiLayout & layout) const
    {
        const Ui::Res::Type::bound_t contentRect = rowsRect();
        const fpx_t                  rowH        = rowHeight();
        if (m_rows.empty() || contentRect.w <= 0.0F || rowH <= 0.0F) {
            return;
        }

        const std::size_t last = lastVisibleRow();
        for (std::size_t i = m_firstRow; i < last; ++i) {
            // Renders but never reports - offset, INVALID_ID would wrap into an
            // id that looks real
            if (m_rows[i].id == INVALID_ID) {
                continue;
            }
            // Offset into the reserved dock-row range so a host row id cannot
            // land on a menu button's. Context subtracts it again, so the host
            // only ever sees its own id.
            const id_t elementId = toDockRowElementId(m_rows[i].id);
            if constexpr (DOCK_DEBUG) {
                const Ui::Res::Type::bound_t sb = sliderBound(contentRect, i);
                std::cout << "[DockColumn::elements] row=" << i << " id=" << m_rows[i].id
                          << " kind=" << static_cast<int>(m_rows[i].kind) << " ratio=" << m_rows[i].ratio
                          << " sliderBound=(" << sb.x << "," << sb.y << " " << sb.w << "x" << sb.h << ")" << std::endl;
            }
            layout.addElement(UiElementType::DockRow, rowBound(contentRect, i), elementId);
            if (m_rows[i].hasChildren) {
                layout.addElement(UiElementType::DockExpander, expanderBound(contentRect, i), elementId);
            }
            // Over its row, so the slider takes the press instead of selecting.
            // The HIT bound, not the track's: what is drawn is taller than the bar
            if (m_rows[i].kind == Ui::Res::Dock::RowKind::Slider) {
                layout.addElement(UiElementType::DockSlider, sliderHitBound(contentRect, i), elementId);
            }
        }
    }

    // A dock row is the same visual object as a menu row, so it takes the popup
    // metrics rather than introducing dock-row layout keys
    [[nodiscard]] fpx_t rowHeight() const { return m_resManager.popup().itemHeight; }

    [[nodiscard]] std::size_t visibleRows() const
    {
        const fpx_t rowH = rowHeight();
        const fpx_t h    = content().h;
        return (rowH > 0.0F && h > 0.0F) ? static_cast<std::size_t>(h / rowH) : 0;
    }

    // Furthest the window can start and still be filled
    [[nodiscard]] std::size_t maxFirstRow() const
    {
        const std::size_t visible = visibleRows();
        return (m_rows.size() > visible) ? m_rows.size() - visible : 0;
    }

    // The [m_firstRow, lastVisibleRow()) window. Both the element pass and the
    // paint pass read it from here: were they to disagree, a click would land on
    // a different row than the one drawn under the cursor
    [[nodiscard]] std::size_t lastVisibleRow() const { return std::min(m_rows.size(), m_firstRow + visibleRows()); }

    bool setFirstRow(int row)
    {
        const auto next = std::clamp(row, 0, static_cast<int>(maxFirstRow()));
        if (next == static_cast<int>(m_firstRow)) {
            return false;
        }
        m_firstRow = static_cast<std::size_t>(next);
        return true;
    }

    // Pixels are what thumb_track_t works in; rows are what actually scrolls
    [[nodiscard]] fpx_t scrollValue() const { return static_cast<fpx_t>(m_firstRow) * rowHeight(); }

    bool setScrollValue(fpx_t value)
    {
        const fpx_t rowH = rowHeight();
        return (rowH > 0.0F) ? setFirstRow(static_cast<int>(std::lround(value / rowH))) : false;
    }

    // The hover width in both states, like the dialog's: the bar grows on hover
    // but its hit area must not, or the pointer would fall out of the widened bar
    [[nodiscard]] fpx_t scrollbarWidth() const { return m_resManager.layout().scrollbarHoverW; }

    // Hand the bar the content area and how many rows there are against how many
    // show. Heights are QUANTISED - visibleRows() * rowHeight(), not content().h:
    // a content area that is not a whole number of rows tall would put the pixel
    // maximum below maxFirstRow(), leaving the last row unreachable by dragging.
    void refreshScrollBar() const
    {
        // The res offset is authored for the dialog, where the bar overhangs into
        // the dialog's padding. A dock has none, so the same offset would hang the
        // bar over the grip - hand it an area shortened by that overhang instead,
        // and it lands flush inside the content.
        const fpx_t            overhang = std::max(0.0F, -m_resManager.layout().scrollbarHoverRight);
        Ui::Res::Type::bound_t area     = content();
        area.w                          = std::max(0.0F, area.w - overhang);

        const fpx_t rowH = rowHeight();
        m_scrollBar.setGeometry(area,
                                static_cast<fpx_t>(m_rows.size()) * rowH,
                                static_cast<fpx_t>(visibleRows()) * rowH);
    }

    // Rows give up the scrollbar gutter, so a right-aligned value never renders
    // under the bar
    [[nodiscard]] Ui::Res::Type::bound_t rowsRect() const
    {
        Ui::Res::Type::bound_t rect = content();
        if (isScrollable()) {
            rect.w = std::max(0.0F, rect.w - scrollbarWidth());
        }
        return rect;
    }

    // Indent step per depth level, derived from the row height so the tree
    // stays proportional at any DPI or font size
    [[nodiscard]] fpx_t indentStep() const { return rowHeight() * m_resManager.layout().dockDefaults.rowIndentRatio; }

    [[nodiscard]] fpx_t expanderSize() const
    {
        return rowHeight() * m_resManager.layout().dockDefaults.rowExpanderRatio;
    }

    // index is absolute in m_rows; the scroll offset turns it into a screen slot.
    // Callers iterate from m_firstRow, so the subtraction never wraps
    [[nodiscard]] Ui::Res::Type::bound_t rowBound(const Ui::Res::Type::bound_t & contentRect, std::size_t index) const
    {
        const fpx_t rowH = rowHeight();
        const auto  slot = static_cast<fpx_t>(index - m_firstRow);
        return { contentRect.x, contentRect.y + (rowH * slot), contentRect.w, rowH };
    }

    // Leading square where the expander sits, inset by the row's indent
    // The draggable track: the row's right-hand column, where a Text row draws
    // its value. Inset by the same padding so a slider lines up with the values
    // above and below it.
    [[nodiscard]] Ui::Res::Type::bound_t sliderBound(const Ui::Res::Type::bound_t & contentRect,
                                                     std::size_t                    index) const
    {
        const Ui::Res::Type::bound_t row  = rowBound(contentRect, index);
        const fpx_t                  padH = m_resManager.popup().itemPaddingH;
        const fpx_t                  w    = (row.w / 2.0F) - (padH * 2.0F);
        const fpx_t                  h    = sliderHeight();
        return { row.x + row.w - w - padH, row.y + ((row.h - h) / 2.0F), w, h };
    }

    // Thin relative to the row, so the label still reads as the dominant thing
    [[nodiscard]] fpx_t sliderHeight() const { return m_resManager.layout().sliderTrackH; }

    // The thumb length the pointer maths uses, which is the HOVER one in both
    // states - a grab is always on a hovered thumb, and sizing the travel to the
    // idle grip would move the value under the pointer the instant it grew
    [[nodiscard]] fpx_t sliderThumbLength() const { return m_resManager.layout().sliderThumbHoverW; }

    [[nodiscard]] std::optional<std::size_t> sliderIndex(id_t rowId) const
    {
        for (std::size_t i = 0; i < m_rows.size(); ++i) {
            if (m_rows[i].id == rowId && m_rows[i].kind == Ui::Res::Dock::RowKind::Slider) {
                return i;
            }
        }
        return std::nullopt;
    }

    /**
     * @brief What the pointer can press to work the slider
     *
     * The track's length, but the HOVER thumb's height, in both states. Sizing
     * this to the track would be sizing it to the thinnest thing drawn: the grip
     * stands several times the track's thickness, so most of what a reader aims
     * at would not answer. The scrollbar states the same rule for its own
     * widening, on the other axis.
     *
     * The track's LENGTH needs no growing - thumbBound keeps the thumb inside the
     * track ends, so nothing is ever drawn past them.
     */
    [[nodiscard]] Ui::Res::Type::bound_t sliderHitBound(const Ui::Res::Type::bound_t & contentRect,
                                                        std::size_t                    index) const
    {
        const Ui::Res::Type::bound_t track = sliderBound(contentRect, index);
        const fpx_t                  h     = std::max(track.h, m_resManager.layout().sliderThumbHoverH);
        return { track.x, track.y + ((track.h - h) / 2.0F), track.w, h };
    }

    /**
     * @brief Where the thumb sits and how big it is, in the state it is drawn in
     *
     * Its position along the track comes from the same metrics the drag reads, so
     * the grip stays under the pointer. Its SIZE is res, per state, which is what
     * lets a hover grow the grip without moving the track under it - the rect is
     * re-centred on the track so it grows about its own middle.
     *
     * Hit-testing passes isHot=true in BOTH states, the same rule the scrollbar
     * states for its own widening: the hit area must be the larger of the two, or
     * the pointer falls out of the grip at the moment the grip grows to meet it.
     */
    [[nodiscard]] Ui::Res::Type::bound_t sliderThumbBound(const Ui::Res::Type::bound_t & track,
                                                          fpx_t                          fraction,
                                                          bool                           isHot) const
    {
        const auto &                 layout = m_resManager.layout();
        const fpx_t                  w      = isHot ? layout.sliderThumbHoverW : layout.sliderThumbW;
        const fpx_t                  h      = isHot ? layout.sliderThumbHoverH : layout.sliderThumbH;
        const Ui::Res::Type::bound_t span   = thumb_track_t::thumbBound(sliderMetrics(track, sliderThumbLength()),
                                                                      fraction);
        return { span.x + ((span.w - w) / 2.0F), track.y + ((track.h - h) / 2.0F), w, h };
    }

    // Which slider thumb the pointer is on, INVALID_ID for none. Walked here
    // rather than read off element state for the same reason the grip and
    // scrollbar cues are: the dock draws from the extra-ops hook, where no
    // element is in hand. A row index rather than a flag per row, because a dock
    // holds many sliders and only one can ever be under the pointer.
    [[nodiscard]] id_t hotSliderRow(fpx_t cssX, fpx_t cssY) const
    {
        const Ui::Res::Type::bound_t contentRect = rowsRect();
        const std::size_t            last        = lastVisibleRow();
        for (std::size_t i = m_firstRow; i < last; ++i) {
            if (m_rows[i].kind != Ui::Res::Dock::RowKind::Slider) {
                continue;
            }
            if (sliderThumbBound(sliderBound(contentRect, i), m_rows[i].ratio, true).contains(cssX, cssY)) {
                return i;
            }
        }
        return INVALID_ID;
    }

    [[nodiscard]] Ui::Res::Type::bound_t expanderBound(const Ui::Res::Type::bound_t & contentRect,
                                                       std::size_t                    index) const
    {
        const Ui::Res::Type::bound_t row  = rowBound(contentRect, index);
        const fpx_t                  size = expanderSize();
        const fpx_t                  x    = row.x + m_resManager.popup().itemPaddingH
                      + (indentStep() * static_cast<fpx_t>(m_rows[index].depth));
        return { x, row.y + ((row.h - size) / 2.0F), size, size };
    }

    // Rows fill the content area top-down, starting at the scroll offset
    void renderRows(UiRenderer & out, const Ui::Res::Type::bound_t & contentRect) const
    {
        if (m_rows.empty() || contentRect.w <= 0.0F) {
            return;
        }

        const auto &      theme = m_resManager.theme();
        const fpx_t       padH  = m_resManager.popup().itemPaddingH;
        const std::size_t last  = lastVisibleRow();
        // The expander glyph fills its box edge to edge, so without this the
        // label starts against it. One space in the row's own font is the gap a
        // reader expects between a glyph and the word after it
        const fpx_t gap = out.textWidth(out.itemFont(), " ");

        for (std::size_t i = m_firstRow; i < last; ++i) {
            const Ui::Res::Dock::row_t & row   = m_rows[i];
            const Ui::Res::Type::bound_t bound = rowBound(contentRect, i);

            // Selected rows take the menu's active block; a dock row and a menu
            // row are the same thing visually, so they share the theming
            if (row.isSelected) {
                out.appendBg(bound, theme.menuItemActive, theme.dock.background.bg);
            }

            const fpx_t indent = padH + (indentStep() * static_cast<fpx_t>(row.depth));

            if (row.hasChildren) {
                const std::string expander = m_resManager
                                             .iconDefault(row.isExpanded ? Ui::Res::Key::IconRoleKey::RowExpanded
                                                                         : Ui::Res::Key::IconRoleKey::RowCollapsed)
                                             .icon;
                if (!expander.empty()) {
                    out.appendImage(expanderBound(contentRect, i),
                                    m_resManager.resPath().icon(expander),
                                    theme.dock.background.fg);
                }
            }

            const Ui::font_handle_t font = row.isSelected ? out.itemFontBold() : out.itemFont();

            // A slider owns the right column, so the value text - if the host
            // supplied one - shifts left to sit between the label and the track
            Ui::Res::Type::bound_t valueBound = bound;
            if (row.kind == Ui::Res::Dock::RowKind::Slider) {
                const Ui::Res::Type::bound_t track = sliderBound(contentRect, i);
                renderSlider(out, track, row.ratio, i == m_hotSliderRow);
                valueBound.w = track.x - bound.x;
            }

            // Halve what is left of the row after its padding: keys own the left
            // half, values the right one, so both columns line up down the dock.
            // The expander and the depth indent are charged to the key half - a
            // deeper row spends its own half on them and the split stays put.
            // A row with no value has no second column to line up with, so it
            // keeps the whole width - a tree name cut at the halfway mark for an
            // empty right half is just a name nobody can read
            const fpx_t                  labelX     = bound.x + indent + expanderSize() + gap;
            const fpx_t                  labelRight = row.value.empty()
                                                    ? bound.x + bound.w - padH
                                                    : bound.x + padH
                                     + ((bound.w - (padH * 2.0F)) * m_resManager.layout().dockDefaults.rowKeyRatio);
            const Ui::Res::Type::bound_t labelBound { labelX, bound.y, labelRight - labelX, bound.h };
            const std::string            label = out.truncate(font, row.label, labelBound.w);
            out.appendText(labelBound, label, font, theme.dock.background.fg);

            // The value keeps its own half whatever the key does, and spends the
            // key's unused remainder before it gives up a character - so a rounded
            // number never loses digits, and only a long text value is ever cut
            const fpx_t       labelEnd = labelX + out.textWidth(font, label) + padH;
            const fpx_t       valueMax = valueBound.x + valueBound.w - padH - std::max(labelEnd, labelRight);
            const std::string value    = out.truncate(font, row.value, valueMax);

            // Property value, right-aligned in the same row. Empty on a tree
            // row, which is what makes one row_t serve both docks
            if (!value.empty()) {
                out.appendText(valueBound,
                               value,
                               font,
                               theme.dock.background.fg,
                               Ui::Res::Type::AlignH::Right,
                               Ui::Res::Type::AlignV::Center,
                               padH);
            }
        }
    }

    // The dialog's scrollbar, in a dock: same res block ("scrollbar" + its hover),
    // same widening on hover, same radius. Only the surround differs - the blend
    // colour is the dock background rather than the dialog's.
    void renderScrollbar(UiRenderer & out) const
    {
        if (!isScrollable()) {
            return;
        }
        const auto & theme = m_resManager.theme();
        refreshScrollBar();

        const Ui::Res::Type::border_t radius = m_scrollBar.radius();
        const Ui::Color &             fg     = theme.dock.background.fg;
        out.appendBg(m_scrollBar.track(), { fg, m_scrollBar.trackColor() }, theme.dock.background.bg, radius);
        out.appendBg(m_scrollBar.thumb(scrollValue()),
                     { fg, m_scrollBar.thumbColor() },
                     m_scrollBar.trackColor(),
                     radius);
    }

    // Track, filled portion, and thumb. `isHot` is the pointer being on the THUMB,
    // decided once per move by hotSliderRow and passed down rather than re-tested
    // here, so the cue and the hit area cannot disagree about where the grip is.
    void renderSlider(UiRenderer & out, const Ui::Res::Type::bound_t & track, fpx_t ratio, bool isHot) const
    {
        const auto & theme    = m_resManager.theme();
        const fpx_t  fraction = std::clamp(ratio, 0.0F, 1.0F);

        if constexpr (DOCK_DEBUG) {
            std::cout << "[DockColumn::slider] track=(" << track.x << "," << track.y << " " << track.w << "x" << track.h
                      << ") ratio=" << fraction << " hot=" << isHot << " trackBg=" << theme.dock.sliderTrack.bg.toHex()
                      << " thumbBg="
                      << (isHot ? theme.dock.sliderThumbHover.bg.toHex() : theme.dock.sliderThumb.bg.toHex())
                      << " dockBg=" << theme.dock.background.bg.toHex() << std::endl;
        }

        const Ui::Res::Type::border_t trackRadius = m_resManager.layout().sliderTrackBorder;
        out.appendBg(track, theme.dock.sliderTrack, theme.dock.background.bg, trackRadius);
        // The filled portion takes the THUMB's colour, not one of its own: it and
        // the thumb are the same idea - where the value is - and colouring them
        // apart would read as two indicators on one bar.
        if (fraction > 0.0F) {
            out.appendBg({ track.x, track.y, track.w * fraction, track.h },
                         theme.dock.sliderThumb,
                         theme.dock.background.bg,
                         trackRadius);
        }

        // Thumb, kept fully inside the track ends so it never overhangs. Its
        // position comes from the same metrics the drag reads, so it sits where
        // the pointer is; only the cross axis is ours, centred on the track.
        //
        // Size and radius come from res per STATE, so hovering can grow the grip
        // without touching the track it rides on - which is why the two are
        // separate blocks rather than one slider block with a shared height.
        out.appendBg(sliderThumbBound(track, fraction, isHot),
                     isHot ? theme.dock.sliderThumbHover : theme.dock.sliderThumb,
                     theme.dock.background.bg,
                     isHot ? m_resManager.layout().sliderThumbHoverBorder : m_resManager.layout().sliderThumbBorder);
    }

    // Toggle the dock collapsed/restored. When expanded, remember the current
    // size into memoryX (so the next restore brings back what the user just
    // had on screen) and collapse to 0. When collapsed, restore memoryX -
    // which is always non-zero thanks to the ctor's defaultWidth seed.
    void applyDoubleClickToggle()
    {
        if (m_state.width > 0.0F) {
            m_state.memoryX = m_state.width;
            m_state.width   = 0.0F;
        } else {
            m_state.width = m_state.memoryX;
        }
        m_resManager.setDockState(m_config.name, m_state);
    }

    id_t m_id;
    Ui::Res::Dock::dock_config_t
    m_config; // owned copy: layout reload can re-emplace m_layout.docks, would dangle a reference
    const Ui::Res::ResManager & m_resManager;
    Ui::Res::Dock::dock_state_t m_state;
    Ui::Res::Type::bound_t      m_outer {}; // CSS-pixel outer rect, refreshed by setLayout()
    bool                        m_isHoveredGrip = false;
    id_t                        m_hotSliderRow  = INVALID_ID; // which thumb the pointer is on
    bool                        m_isDragging    = false;
    drag_track_t                m_grip;               // grab state while m_isDragging
    fpx_t                       m_transientWidth = 0; // live width during drag
    thumb_track_t               m_sliderDrag;         // grab state of the slider being dragged
    thumb_metrics_t             m_sliderMetrics;      // that slider's track at the grab

    std::vector<Ui::Res::Dock::row_t> m_rows;         // host-projected content
    std::size_t                       m_firstRow = 0; // top row of the scroll window
    // mutable: refreshScrollBar() restates geometry from row data during const
    // paint and element passes, the way text measurement does elsewhere
    mutable Ui::Render::ScrollBar m_scrollBar { m_resManager };
};

} // namespace Ui::Render
