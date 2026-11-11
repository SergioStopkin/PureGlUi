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

#include "ui/io/filefilter.h"
#include "ui/io/platform/command.h"
#include "ui/io/platform/portalconst.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui::Io::Platform {

// zenity's (GNOME/GTK) and kdialog's (KDE) open dialog, where no portal answers. Each an argv of
// its own: no shell runs them, so a quote in a folder or a title is one more character of its
// argument. Both pick several files from the filters.
//
// Under an X11 parent both are attached to it, as clients of its X server - on a Wayland desktop
// the only way to reach the window - so each stands above it and keeps it from the focus
inline std::array<command_t, 2> dialogCommandsOf(const std::string &                parentWindow,
                                                 const std::string &                startDir,
                                                 const std::string &                title,
                                                 const std::vector<file_filter_t> & filters)
{
    const std::string dir        = startDir.empty() ? "." : startDir;
    const std::string shownTitle = title.empty() ? "Open File" : title;
    command_t         zenity { .args        = { "zenity",
                                                "--file-selection",
                                                "--title=" + shownTitle,
                                                "--multiple",
                                                "--separator=\n",
                                                "--filename=" + dir + "/" },
                               .environment = {} };
    std::string       kdialogFilter;
    for (const file_filter_t & filter : filters) {
        std::string patterns = filter.spec;
        std::ranges::replace(patterns, ';', ' ');
        zenity.args.emplace_back("--file-filter=" + filter.name + "|" + patterns);
        kdialogFilter += (kdialogFilter.empty() ? "" : "\n") + patterns + "|" + filter.name;
    }
    command_t kdialog {
        .args        = { "kdialog", "--title", shownTitle, "--getopenfilename", "--multiple", dir, kdialogFilter },
        .environment = {}
    };

    constexpr std::string_view X11 = Ui::Io::Platform::Portal::X11_PREFIX;
    if (parentWindow.starts_with(X11)) {
        const std::string_view hex    = std::string_view(parentWindow).substr(X11.size());
        std::uint64_t          window = 0;
        std::from_chars(std::to_address(hex.begin()), std::to_address(hex.end()), window, 16);
        const std::string attach = "--attach=" + std::to_string(window);
        zenity.args.emplace_back("--modal");
        zenity.args.emplace_back(attach);
        zenity.environment.emplace_back("GDK_BACKEND=x11");
        kdialog.args.emplace_back(attach);
        kdialog.environment.emplace_back("QT_QPA_PLATFORM=xcb");
    }
    return { std::move(zenity), std::move(kdialog) };
}

} // namespace Ui::Io::Platform
