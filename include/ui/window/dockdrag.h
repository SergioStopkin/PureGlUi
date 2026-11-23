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
#include "ui/idkind.h"
#include "ui/render/dockcolumn.h"
#include "ui/render/uielement.h"
#include "ui/render/uilayout.h"
#include "ui/type.h"
#include "ui/window/dockaction.h"
#include "ui/window/docks.h"

#include <functional>
#include <optional>
#include <utility>

namespace Ui::Window {

// A press on a dock's grip, slider or scrollbar, held until its release: the element that
// declared DragStart takes every move and the release, chosen by its accepts mask rather
// than by type, even when the cursor wanders into the content surface or another area.
// The consequence is chrome - a dock resize, the way tab-arrow scrolling is - so it is
// handled here rather than through the intent table, and handed back as a DockAction
class DockDrag final : private Common::NonCopyable {
public:
    explicit DockDrag(Docks & docks)
        : m_docks(docks)
    {
    }

    // Host reaction to a slider drag: (row id, 0..1). A continuous stream, so it is a hook
    // rather than an intent - the host owns the value, re-projects its rows, and the dock
    // repaints from that
    void setOnRowValue(std::function<void(id_t, fpx_t)> callback) { m_onRowValue = std::move(callback); }

    // Whether a gesture holds the pointer
    [[nodiscard]] bool isHolding() const { return m_element != Ui::INVALID_ID; }

    // A left press on `element`, which declared DragStart. Both axes are passed on: the
    // grip and slider drag horizontally, the scrollbar vertically. Pass for an element
    // this knows no gesture of
    DockAction begin(const Ui::Render::UiElement & element, fpx_t cssX, fpx_t cssY, int clickCount)
    {
        m_type = element.type;

        if (element.type == Ui::Render::UiElementType::DockSlider) {
            const id_t sliderDock = m_docks.idWhere(
                [&element](const Ui::Render::DockColumn & dock) { return dock.hasSliderRow(element.id); });
            std::optional<fpx_t> jumped;
            if (!m_docks.visit(sliderDock, [&jumped, &element, cssX](Ui::Render::DockColumn & dock) {
                    jumped = dock.beginSliderGesture(element.id, cssX);
                })) {
                return DockAction::Pass;
            }
            m_element = element.id;
            m_dock    = sliderDock;
            if (jumped.has_value()) {
                reportRowValue(*jumped);
            }
            return DockAction::Consume;
        }

        // Pressing the track pages instead of grabbing, so the gesture can be over
        // before it starts - isDraggingScroll() is what decides whether to capture
        if (element.type == Ui::Render::UiElementType::DockScrollbar) {
            bool       isDraggingScroll = false;
            const bool isHeld           = m_docks.visit(toDockId(element.id),
                                                        [&isDraggingScroll, cssY](Ui::Render::DockColumn & dock) {
                                                  dock.beginScrollGesture(cssY);
                                                  isDraggingScroll = dock.isDraggingScroll();
                                                        });
            if (!isHeld) {
                return DockAction::Pass;
            }
            if (isDraggingScroll) {
                m_element = element.id;
            }
            return DockAction::Refresh;
        }

        if (element.type != Ui::Render::UiElementType::DockGrip) {
            return DockAction::Pass;
        }
        bool       isDragging = false;
        const bool isHeld     = m_docks.visit(toDockId(element.id),
                                              [&isDragging, cssX, clickCount](Ui::Render::DockColumn & dock) {
                                              dock.beginGripGesture(cssX, clickCount);
                                              isDragging = dock.isDragging();
                                              });
        if (!isHeld) {
            return DockAction::Pass;
        }
        // Double-click resolves immediately (no drag), so nothing is held. A normal press
        // keeps the pointer until its release.
        if (isDragging) {
            m_element = element.id;
            return DockAction::Consume;
        }
        // Restore via memoryX may exceed what fits now (other docks may have grown since
        // the size was captured), so clamp against current viewport availability before
        // the layout pass sees it.
        m_docks.visit(toDockId(element.id), [this](Ui::Render::DockColumn & dock) { m_docks.clampState(dock); });
        return DockAction::Reflow;
    }

    // A move to whatever holds the pointer, ignoring what is under the cursor - that is the
    // whole point of capture. Pass, the pointer let go, where its dock went away
    DockAction move(fpx_t cssX, fpx_t cssY)
    {
        if (m_type == Ui::Render::UiElementType::DockSlider) {
            fpx_t ratio = 0.0F;
            if (!m_docks.visit(m_dock,
                               [&ratio, cssX](Ui::Render::DockColumn & dock) { ratio = dock.onSliderDrag(cssX); })) {
                m_element = Ui::INVALID_ID;
                return DockAction::Pass;
            }
            reportRowValue(ratio);
            return DockAction::Consume;
        }

        if (m_type == Ui::Render::UiElementType::DockScrollbar) {
            bool isScrolled = false;
            if (!m_docks.visit(toDockId(m_element), [&isScrolled, cssY](Ui::Render::DockColumn & dock) {
                    isScrolled = dock.onScrollDrag(cssY);
                })) {
                m_element = Ui::INVALID_ID;
                return DockAction::Pass;
            }
            return isScrolled ? DockAction::Refresh : DockAction::Consume;
        }

        bool isResized = false;
        if (!m_docks.visit(toDockId(m_element), [this, &isResized, cssX, cssY](Ui::Render::DockColumn & dock) {
                // Transient width changed. Clamp to what the viewport can actually
                // fit (so the dock cannot push the content surface below zero)
                if (dock.onMouseMove(cssX, cssY)) {
                    m_docks.clampDragging(dock);
                    isResized = true;
                }
            })) {
            m_element = Ui::INVALID_ID;
            return DockAction::Pass;
        }
        // The content child shrinks/grows live as the user drags
        return isResized ? DockAction::Reflow : DockAction::Consume;
    }

    // The release, at `cssX`: the pointer let go. A slider and a scrollbar commit on every
    // move, so their release only ends the grab; a grip commits its width now
    DockAction end(fpx_t cssX)
    {
        DockAction action = DockAction::Consume;
        if (m_type == Ui::Render::UiElementType::DockSlider) {
            m_docks.visit(m_dock, [](Ui::Render::DockColumn & dock) { dock.endSliderDrag(); });
        } else if (m_type == Ui::Render::UiElementType::DockScrollbar) {
            m_docks.visit(toDockId(m_element), [](Ui::Render::DockColumn & dock) { dock.endScrollDrag(); });
        } else {
            m_docks.visit(toDockId(m_element), [cssX](Ui::Render::DockColumn & dock) { dock.onMouseUp(cssX); });
            action = DockAction::Reflow;
        }
        m_element = Ui::INVALID_ID;
        return action;
    }

    // The pointer let go with no release to commit it: a leave (the docks' own leave
    // settles the width) or a layout reload, whose docks a stale id must not route into
    void cancel() { m_element = Ui::INVALID_ID; }

private:
    Docks &                          m_docks;
    std::function<void(id_t, fpx_t)> m_onRowValue;
    id_t                             m_element = Ui::INVALID_ID; // the element holding the pointer
    id_t                             m_dock = Ui::INVALID_ID; // the dock a slider drag belongs to: its row names none
    Ui::Render::UiElementType        m_type = Ui::Render::UiElementType::DockGrip;

    void reportRowValue(fpx_t ratio)
    {
        if (m_onRowValue) {
            m_onRowValue(m_element, ratio);
        }
    }
};

} // namespace Ui::Window
