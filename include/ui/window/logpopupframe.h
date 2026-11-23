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

#include "ui/gl/localglew.h"
#include "ui/res/type/bound.h"
#include "ui/window/popup/popupwindow.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

namespace Ui::Window {

// A/B: how much of a popup's own frame, `bound` big and just drawn, is part see-through,
// as presented - under POPUP_DEBUG
inline void logPopupFrame(const Ui::Res::Type::bound_t & bound)
{
    if constexpr (!Ui::Window::Popup::POPUP_DEBUG) {
        return;
    }
    const int            frameW = static_cast<int>(bound.w);
    const int            frameH = static_cast<int>(bound.h);
    std::vector<uint8_t> rgba(static_cast<size_t>(frameW) * frameH * 4);
    glReadPixels(0, 0, frameW, frameH, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    int partial  = 0;
    int minAlpha = 255;
    for (size_t idx = 3; idx < rgba.size(); idx += 4) {
        if (rgba.at(idx) > 0 && rgba.at(idx) < 255) {
            ++partial;
            minAlpha = std::min<int>(minAlpha, rgba.at(idx));
        }
    }
    GLint encoding = 0;
    GLint samples  = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER,
                                          GL_BACK_LEFT,
                                          GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,
                                          &encoding);
    glGetIntegerv(GL_SAMPLES, &samples);
    std::cout << "[A/B] sRGB on=" << static_cast<int>(glIsEnabled(GL_FRAMEBUFFER_SRGB))
              << " encoding=" << (encoding == GL_SRGB ? "sRGB" : "linear") << " samples=" << samples
              << " a2c=" << static_cast<int>(glIsEnabled(GL_SAMPLE_ALPHA_TO_COVERAGE)) << " ";
    std::cout << "[A/B] popup " << frameW << "x" << frameH << ": " << partial << " part see-through, lowest "
              << minAlpha << ", row 30 from x 34:";
    const int row = frameH - 1 - 30;
    for (int column = 34; column < 74 && column < frameW; ++column) {
        const size_t at = ((static_cast<size_t>(row) * frameW) + column) * 4;
        std::cout << " " << ((rgba.at(at) + rgba.at(at + 1) + rgba.at(at + 2)) / 3);
    }
    std::cout << std::endl;
}

} // namespace Ui::Window
