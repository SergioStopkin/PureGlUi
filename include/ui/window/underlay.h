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
#include "ui/color.h"
#include "ui/config.h"
#include "ui/gl/localglew.h"
#include "ui/gl/rounded.h"
#include "ui/res/type/border.h"
#include "ui/res/type/bound.h"
#include "ui/type.h"
#include "ui/window/clampedradiiof.h"
#include "ui/window/connector/mainconnector.h"
#include "ui/window/connector/pairing.h"
#include "ui/window/geometry.h"
#include "ui/window/hostsurfaces.h"
#include "ui/window/popupslot.h"
#include "ui/window/underlayof.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace Ui::Window {

// What stands under a popup-type window - the main window's back buffer, and a content
// surface that is a window of its own - read for its software-rounded corners, or for
// its whole rect where its background lets it show through
class Underlay final : private Common::NonCopyable {
public:
    // All the window manager's, `main` empty until it makes the window
    Underlay(const std::unique_ptr<MainConnector> & main, const Geometry & geometry, const HostSurfaces & hostSurfaces)
        : m_main(main)
        , m_geometry(geometry)
        , m_hostSurfaces(hostSurfaces)
    {
    }

    // Recapture the parent-behind-the-corners pixels and push them into the
    // popup/dialog renderer for software-rounded corners - or, for a popup whose
    // background is see-through, the pixels behind its whole rect. Works on any
    // popup-type connector (its renderer derives PopupRendererBase). Call after the main
    // frame is drawn and before its swap, so the back buffer holds what stands under it
    template <typename TConnector>
    void recapture(popup_slot_t<TConnector> & slot)
    {
        if (!slot.connector || !slot.connector->hasRenderer()) {
            return;
        }
        TConnector & connector = *slot.connector;

        const auto &    cb         = connector.window().bound();
        const Ui::Color background = connector.window().background();

        // A see-through background shows what stands under the whole popup, not only its corners
        if (background.a() < 255) {
            const std::vector<uint8_t> underlay = read(cb, background);
            connector.window().makeCurrent();
            connector.renderer().setUnderlay(underlay, static_cast<int>(cb.w), static_cast<int>(cb.h));
        } else {
            const Ui::Res::Type::border_t       radii   = clampedRadiiOf(connector.window().cornerRadii(), cb.w, cb.h);
            std::array<std::vector<uint8_t>, 4> corners = readCorners(cb, radii, background);
            connector.window().makeCurrent();
            connector.renderer().setCornerPixels(std::move(corners), radii);
        }
        connector.pairing().render();

        // Composited, it is read now and the main frame that recaptured it draws it
        // in; otherwise its own frame presents it
        if (g_config.isCompositing) {
            slot.composite.capture(connector.window().bound());
            slot.isRecaptured = true;
        } else {
            connector.window().requestRender();
        }

        m_main->window().makeCurrent();
    }

private:
    const std::unique_ptr<MainConnector> & m_main;
    const Geometry &                       m_geometry;
    const HostSurfaces &                   m_hostSurfaces;

    // What stands under each corner of a popup at `popupBound` with `radii`, its own square
    // (TL=0, TR=1, BR=2, BL=3) - none for a square corner. RGBA rows from the top
    [[nodiscard]] std::array<std::vector<uint8_t>, 4> readCorners(const Ui::Res::Type::bound_t &  popupBound,
                                                                  const Ui::Res::Type::border_t & radii,
                                                                  const Ui::Color &               bgColor)
    {
        const int                           popX = static_cast<int>(popupBound.x);
        const int                           popY = static_cast<int>(popupBound.y);
        const int                           popW = static_cast<int>(popupBound.w);
        const int                           popH = static_cast<int>(popupBound.h);
        std::array<std::vector<uint8_t>, 4> corners;
        for (id_t i = 0; i < 4; ++i) {
            const auto radius = static_cast<int>(Ui::Gl::Rounded::borderRadius(radii, i));
            if (radius <= 0) {
                continue;
            }
            const bool isRight  = i == 1 || i == 2;
            const bool isBottom = i == 2 || i == 3;
            corners.at(i)       = read({ .x = static_cast<fpx_t>(isRight ? popX + popW - radius : popX),
                                         .y = static_cast<fpx_t>(isBottom ? popY + popH - radius : popY),
                                         .w = static_cast<fpx_t>(radius),
                                         .h = static_cast<fpx_t>(radius) },
                                       bgColor);
        }
        return corners;
    }

    // What stands under `area` (main-window physical px) - a corner's square, or the whole rect of
    // a popup whose background lets it show through: RGBA rows from the top, the area's size,
    // `bgColor` where it runs off the main window. Read from the main back buffer; a content
    // surface that is a window of its own - not composited - is read for its overlap from its
    // renderer, only the pixels it needs, never the frame. Leaves the main window's context current
    [[nodiscard]] std::vector<uint8_t> read(const Ui::Res::Type::bound_t & area, const Ui::Color & bgColor)
    {
        if (!m_main) {
            return {};
        }
        m_main->window().makeCurrent();

        // Composited, the main frame drew the content surface into the buffer read; else one that
        // is a window of its own is read for its overlap from its renderer
        const std::optional<Ui::Res::Type::bound_t> contentBound = g_config.isCompositing
                                                                     ? std::nullopt
                                                                     : m_hostSurfaces.activeReadyBound();
        const auto                                  content      = m_hostSurfaces.activePairing();
        const int                                   windowHeight = static_cast<int>(m_geometry.height());
        const std::vector<uint8_t>                  pixels       = Ui::Window::underlayOf(
            area,
            static_cast<int>(m_geometry.width()),
            windowHeight,
            bgColor,
            [windowHeight](const Ui::Res::Type::bound_t & region) {
                const int            width  = static_cast<int>(region.w);
                const int            height = static_cast<int>(region.h);
                std::vector<uint8_t> read(static_cast<size_t>(width) * height * 4);
                glReadPixels(static_cast<int>(region.x),
                             windowHeight - static_cast<int>(region.y) - height,
                             width,
                             height,
                             GL_RGBA,
                             GL_UNSIGNED_BYTE,
                             read.data());
                return read;
            },
            contentBound,
            [content](const Ui::Res::Type::bound_t & region) {
                return content.has_value() ? content->get().readPixels(region) : std::vector<uint8_t> {};
            });
        m_main->window().makeCurrent();
        return pixels;
    }
};

} // namespace Ui::Window
