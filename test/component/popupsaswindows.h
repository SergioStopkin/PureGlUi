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

// Lists and dialogs as windows of their own while one lives, made before the shell starts: an X11
// build that finds WAYLAND_DISPLAY takes itself for XWayland and draws them into the main window.
// Elsewhere they are windows of their own already

#include "common/noncopyable.h"

#ifdef HAVE_X11
#include "common/system.h"

#include <cstdlib>
#include <string>
#endif

namespace PureGlUi {

class PopupsAsWindows final : private Common::NonCopyable {
#ifdef HAVE_X11
public:
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    PopupsAsWindows() { unsetenv(VARIABLE); }

    ~PopupsAsWindows()
    {
        if (!m_waylandDisplay.empty()) {
            // NOLINTNEXTLINE(concurrency-mt-unsafe)
            setenv(VARIABLE, m_waylandDisplay.c_str(), 1);
        }
    }

private:
    static constexpr const char * VARIABLE = "WAYLAND_DISPLAY";

    std::string m_waylandDisplay = Common::System::environmentVariable(VARIABLE); // as found, put back
#endif
};

} // namespace PureGlUi
