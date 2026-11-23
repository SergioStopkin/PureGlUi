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

#include "common/system.h"
#include "common/unicode.h"
#include "ui/gl/glutil.h"
#include "ui/gl/localglew.h"
#include "ui/window/displaystextof.h"
#include "ui/window/nativewindow.h"

#include <string>

namespace Ui {

// Dialog content with its %KEY% placeholders expanded (the System Info / Version
// dialogs): %VERSION% from APP_VERSION, %CPU%/%RAM% from Common::System,
// %GPU%/%GL% from `mainWindow`'s live GL context, made current for them,
// %DISPLAYS% one line per display
inline std::wstring resolvedContentOf(const std::string & content, Ui::Window::NativeWindow & mainWindow)
{
    std::wstring result = Common::Unicode::fromUtf8(content);

    auto replace = [&result](const std::wstring & key, const std::wstring & value) {
        auto pos = result.find(key);
        while (pos != std::wstring::npos) {
            result.replace(pos, key.size(), value);
            pos = result.find(key, pos + value.size());
        }
    };

    replace(L"%VERSION%", L"" APP_VERSION);

    std::string       cpu  = std::to_string(Common::System::cpuCores()) + " cores";
    const std::string freq = Common::System::cpuFrequency();
    if (freq != "N/A") {
        cpu += " @ " + freq;
    }
    replace(L"%CPU%", Common::Unicode::fromUtf8(cpu));

    // GL info (requires current context)
    mainWindow.makeCurrent();
    const std::string gpuStr = Ui::Gl::Util::glString(GL_RENDERER);
    std::string       glStr  = Ui::Gl::Util::glString(GL_VERSION);
    // isHardwareGl() is authoritative: WindowManager folds the GL_RENDERER
    // software-rasterizer check into it right after context creation.
    glStr += mainWindow.isHardwareGl() ? " (hardware)" : " (software)";
    replace(L"%GPU%", Common::Unicode::fromUtf8(gpuStr));
    replace(L"%GL%", Common::Unicode::fromUtf8(glStr));

    replace(L"%RAM%", Common::Unicode::fromUtf8(Common::System::systemRam()));

    // Asked again here, not kept from startup: a display plugged in since is listed
    replace(L"%DISPLAYS%",
            Common::Unicode::fromUtf8(
                Ui::Window::displaysTextOf(Ui::Window::NativeWindow::displaysOf(mainWindow.nativeDisplay()))));

    return result;
}

} // namespace Ui
