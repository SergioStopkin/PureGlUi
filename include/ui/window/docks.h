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
#include "ui/config.h"
#include "ui/gl/svgdocuments.h"
#include "ui/idkind.h"
#include "ui/linkedmap.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/uilayout.h"
#include "ui/render/uirenderer.h"
#include "ui/res/dock/anchor.h"
#include "ui/res/dock/row.h"
#include "ui/res/resmanager.h"
#include "ui/type.h"
#include "ui/window/geometry.h"
#include "ui/window/outermostfirstof.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Ui::Window {

// The dock columns (left/right collapsible panels), one per configured one. Keyed by an
// IdKind::Dock id so the key doubles as the Subscribe source id for state-change events,
// and held outermost first (outermostFirstOf), so every walk runs in one order and
// layout packs them as it walks
class Docks final : private Common::NonCopyable {
public:
    Docks(const Ui::Res::ResManager & resManager, const Geometry & geometry)
        : m_resManager(resManager)
        , m_geometry(geometry)
    {
    }

    // One dock per configured one, outermost first, each its state from the session (the
    // DockColumn constructor); its id the Dock kind over its place in the config. The grip
    // glyph preloaded, so DockColumn::render's first size query against SvgDocuments
    // hits rather than falling back to a square
    void make()
    {
        m_columns.clear();
        const auto & dockConfigs = m_resManager.layout().docks;
        for (const std::size_t index : outermostFirstOf(dockConfigs)) {
            const id_t id = Ui::idOf(Ui::IdKind::Dock, index);
            m_columns.emplace(id, id, dockConfigs.at(index), m_resManager);
        }
        if (!m_columns.empty()) {
            const std::string & iconName = m_resManager.layout().dockDefaults.gripIcon;
            if (!iconName.empty()) {
                Ui::Gl::SvgDocuments::loadFilledFromFile(m_resManager.resPath().icon(iconName));
            }
        }
    }

    // Made again for a layout that changed, the host's rows carried over by dock name: a
    // dock the reload removed simply has no taker; one it added comes up empty until the
    // host projects into it. Each clamped to the window as it is now
    void remake()
    {
        std::unordered_map<std::string, std::vector<Ui::Res::Dock::row_t>> carriedRows;
        m_columns.forEach([&carriedRows](id_t /*id*/, const Ui::Render::DockColumn & dock) {
            if (!dock.rows().empty()) {
                carriedRows.emplace(dock.name(), dock.rows());
            }
        });

        make();
        m_columns.forEach([this, &carriedRows](id_t /*id*/, Ui::Render::DockColumn & dock) {
            const auto carried = carriedRows.find(dock.name());
            if (carried != carriedRows.end()) {
                dock.setRows(std::move(carried->second));
            }
            clampState(dock);
        });
    }

    // Every committed (session-persisted) width clamped to what the window fits now: a
    // restored session or a smaller window can leave them summing to more than the
    // viewport takes
    void clampToWindow()
    {
        m_columns.forEach([this](id_t /*id*/, Ui::Render::DockColumn & dock) { clampState(dock); });
    }

    // Clamp the dragging dock's transient width to what the viewport can
    // actually fit right now. Caller has already updated the dock's
    // transient via its onMouseMove; this is the second pass that enforces
    // the dynamic ceiling before reflow.
    void clampDragging(Ui::Render::DockColumn & dock) const { dock.clampTransientWidth(maxContentWidth(dock)); }

    // Same ceiling, but applied to the committed (session-persisted) width.
    // Called after a non-drag state change (double-click restore) so the
    // freshly-revealed dock can't overlap others or push content surface below zero.
    void clampState(Ui::Render::DockColumn & dock) const { dock.clampStateWidth(maxContentWidth(dock)); }

    // Position each dock's outer rect. Each dock is anchored to its toolbar-
    // facing edge (away from the viewport): a left dock's left edge sits
    // against the left toolbar or the next outer dock; growing it pushes the
    // right (viewport-facing) edge inward. Symmetric on the right side.
    //
    // That anchoring is what makes drag-to-resize feel right: during a drag
    // only the dragging dock's currentWidth() changes, but the toolbar-facing
    // edge stays put, so the gesture-followed edge is the grip itself rather
    // than some far edge.
    //
    // One walk, outermost first, positions each edge's outermost against its toolbar,
    // then each inner dock packs against the previous. CSS units throughout -
    // Ui::Render::UiRenderer's draw ops are CSS-native.
    void layOut()
    {
        const auto & layout  = m_resManager.layout();
        const fpx_t  bodyTop = toCss(m_geometry.top());
        const fpx_t  bodyH   = std::max(1.0F, toCss(m_geometry.height() - m_geometry.top() - m_geometry.bottom()));

        // A left dock packs rightward from the left toolbar's inner edge, its
        // innerEdgeX its viewport-facing (right) edge; a right dock leftward from the
        // right toolbar's, its innerEdgeX its left edge
        fpx_t leftEdge  = layout.leftToolbar.width;
        fpx_t rightEdge = toCss(m_geometry.width()) - layout.rightToolbar.width;
        m_columns.forEach([bodyTop, bodyH, &leftEdge, &rightEdge](id_t /*id*/, Ui::Render::DockColumn & dock) {
            if (dock.anchor() == Ui::Res::Dock::DockAnchor::Left) {
                leftEdge += dock.currentWidth();
                dock.setLayout(bodyTop, bodyH, leftEdge);
            } else {
                rightEdge -= dock.currentWidth();
                dock.setLayout(bodyTop, bodyH, rightEdge);
            }
        });
    }

    // The first dock, outermost first, `isWanted` holds for; Ui::INVALID_ID for none
    template <typename Pred>
    [[nodiscard]] id_t idWhere(const Pred & isWanted) const
    {
        id_t found = Ui::INVALID_ID;
        m_columns.forEach([&found, &isWanted](id_t id, const Ui::Render::DockColumn & dock) {
            if (found == Ui::INVALID_ID && isWanted(dock)) {
                found = id;
            }
        });
        return found;
    }

    // `rows` into the dock res calls `dockName`; whether there is one
    bool setRows(std::string_view dockName, std::vector<Ui::Res::Dock::row_t> rows)
    {
        return m_columns.visit(idNamed(dockName),
                               [&rows](Ui::Render::DockColumn & dock) { dock.setRows(std::move(rows)); });
    }

    // The dock res calls `dockName` scrolled back to the top; whether there is one
    bool resetScroll(std::string_view dockName)
    {
        return m_columns.visit(idNamed(dockName), [](Ui::Render::DockColumn & dock) { dock.resetScroll(); });
    }

    // The res menu a dock Menu row names, empty when the element is not one. Every
    // dock is asked, since an element carries the row and not the dock it came from
    [[nodiscard]] std::string menuKeyOf(id_t elementId) const
    {
        std::string key;
        m_columns.forEach([&key, elementId](id_t /*id*/, const Ui::Render::DockColumn & dock) {
            if (key.empty()) {
                key = dock.menuKeyOf(elementId);
            }
        });
        return key;
    }

    // Every dock's draw ops onto `out`, the main window's extra ops
    void render(Ui::Render::UiRenderer & out) const
    {
        m_columns.forEach([&out](id_t /*id*/, const Ui::Render::DockColumn & dock) { dock.render(out); });
    }

    // Every dock's elements into `layout`, the main window's extra elements
    void appendElements(Ui::Render::UiLayout & layout) const
    {
        m_columns.forEach([&layout](id_t /*id*/, const Ui::Render::DockColumn & dock) { dock.appendElements(layout); });
    }

    // The dock `id` handed to `fn`; whether there is one
    template <typename Fn>
    bool visit(id_t id, const Fn & fn)
    {
        return m_columns.visit(id, fn);
    }

    // Every dock, outermost first, handed to fn(id, dock)
    template <typename Fn>
    void forEach(const Fn & fn)
    {
        m_columns.forEach(fn);
    }

    template <typename Fn>
    void forEach(const Fn & fn) const
    {
        m_columns.forEach(fn);
    }

    [[nodiscard]] std::size_t size() const { return m_columns.size(); }

    void clear() { m_columns.clear(); }

private:
    const Ui::Res::ResManager &                 m_resManager; // the DockColumns hold references into it too
    const Geometry &                            m_geometry;
    Ui::LinkedMap<id_t, Ui::Render::DockColumn> m_columns;

    // The dock res calls `dockName`, the routing key, since numeric dock ids are given again
    // on a reload; Ui::INVALID_ID for none
    [[nodiscard]] id_t idNamed(std::string_view dockName) const
    {
        return idWhere([dockName](const Ui::Render::DockColumn & dock) { return dock.name() == dockName; });
    }

    // Maximum content width this dock can take right now without pushing
    // the content surface below zero: workspace - other docks' outer rects -
    // this dock's grip. Used by both the drag-time and the double-click
    // restore clamps; they differ only in which width they cap.
    [[nodiscard]] fpx_t maxContentWidth(const Ui::Render::DockColumn & dock) const
    {
        const auto & layout    = m_resManager.layout();
        const fpx_t  windowCss = toCss(m_geometry.width());
        fpx_t        othersSum = 0.0F;
        m_columns.forEach([&dock, &othersSum](id_t /*id*/, const Ui::Render::DockColumn & other) {
            if (&other != &dock) {
                othersSum += other.currentWidth();
            }
        });
        const fpx_t available = windowCss - layout.leftToolbar.width - layout.rightToolbar.width - othersSum;
        return std::max(0.0F, available - layout.dockDefaults.gripWidth);
    }
};

} // namespace Ui::Window
