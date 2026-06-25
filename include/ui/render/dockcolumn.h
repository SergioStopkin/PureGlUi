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

#include "ui/gl/svgrenderer.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/config.h"
#include "ui/res/dock/state.h"
#include "ui/res/resmanager.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"

#include <iostream>
#include <string>

namespace Ui::Render {

constexpr bool DOCK_DEBUG = false;

// One collapsible dock column anchored to the left or right edge of the
// viewport. Owns the live state (current width + expanded flag) for one
// dock; reads/writes that state through ResManager so session persistence
// is transparent. Rendering and mouse-routing are deliberately absent in
// this revision - they land alongside the grip glyph and the drag-to-
// resize handler in the next step.
class DockColumn final {
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

    DockColumn(const DockColumn &)             = delete;
    DockColumn(DockColumn &&)                  = default;
    DockColumn & operator=(const DockColumn &) = delete;
    DockColumn & operator=(DockColumn &&)      = delete;
    ~DockColumn()                              = default;

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
            const fpx_t cursorDelta = cssX - m_dragStartCursor;
            const fpx_t delta       = (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) ? cursorDelta : -cursorDelta;
            fpx_t       w           = m_dragStartWidth + delta;
            if (w < 0.0F) {
                w = 0.0F;
            }
            const fpx_t widthDelta = w - m_dragStartWidth;
            std::cout << "[DockColumn::drag] name=\"" << m_config.name
                      << "\" anchor=" << (m_config.anchor == Ui::Res::Dock::DockAnchor::Left ? "L" : "R")
                      << " cssX=" << cssX << " startCursor=" << m_dragStartCursor << " cursorDelta=" << cursorDelta
                      << " widthDelta=" << widthDelta << " newW=" << w << std::endl;
            const bool changed = (m_transientWidth != w);
            m_transientWidth   = w;
            return changed;
        }
        const bool nowHover = isOverGrip(cssX, cssY);
        if (nowHover != m_isHoveredGrip) {
            m_isHoveredGrip = nowHover;
            return true;
        }
        return false;
    }

    // Returns true if the press hit the grip (caller should capture).
    // clickCount comes straight from the platform event layer: 1 for a
    // single press, 2 for a double, etc. Double-click on the grip is the
    // toggle gesture (collapse <-> restore memoryX); we resolve it inline
    // and skip starting a drag so the release doesn't commit a tiny width.
    bool onMouseDown(fpx_t cssX, fpx_t cssY, int clickCount)
    {
        if (!isOverGrip(cssX, cssY)) {
            return false;
        }
        if (clickCount >= 2) {
            applyDoubleClickToggle();
            return true;
        }
        m_isDragging      = true;
        m_dragStartCursor = cssX;
        // Start from the live committed width (0 when collapsed). This is
        // what gives "smooth from 0" - the drag composes cursor delta onto
        // whatever the dock is showing right now, not onto a remembered
        // restore size.
        m_dragStartWidth = m_state.width;
        m_transientWidth = m_dragStartWidth;
        return true;
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
        const auto & defaults     = m_resManager.layout().dockDefaults;
        const fpx_t  dragDistance = (m_config.anchor == Ui::Res::Dock::DockAnchor::Left) ? (cssX - m_dragStartCursor)
                                                                                         : (m_dragStartCursor - cssX);
        const fpx_t  absDist      = dragDistance < 0.0F ? -dragDistance : dragDistance;
        m_isDragging              = false;

        if (absDist < defaults.clickThreshold) {
            return;
        }

        m_state.width = m_transientWidth;
        if (m_transientWidth > 0.0F) {
            m_state.memoryX = m_transientWidth;
        }
        m_resManager.setDockState(m_config.name, m_state);
    }

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
            const fpx_t  widthDelta = m_transientWidth - m_dragStartWidth;
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
        out.appendImage(glyph, svgPath, gripColors.fg);
    }

private:
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
    bool                        m_isHoveredGrip   = false;
    bool                        m_isDragging      = false;
    fpx_t                       m_dragStartCursor = 0; // CSS cursor x when drag began
    fpx_t                       m_dragStartWidth  = 0; // m_state.width at drag start
    fpx_t                       m_transientWidth  = 0; // live width during drag
};

} // namespace Ui::Render
