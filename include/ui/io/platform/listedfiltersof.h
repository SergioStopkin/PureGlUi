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

#include <vector>

namespace Ui::Io::Platform {

// Any file, the choice every Linux dialog ends its list with. Made when asked for, never a
// static whose strings could throw before main
inline Ui::Io::file_filter_t allFiles() { return { .name = "All files", .spec = "*" }; }

// What a Linux dialog lists: the caller's filters in their order, then any file - added here
// alone, so the portal and the fallback dialogs list the same
inline std::vector<Ui::Io::file_filter_t> listedFiltersOf(const std::vector<Ui::Io::file_filter_t> & filters)
{
    std::vector<Ui::Io::file_filter_t> listed = filters;
    listed.emplace_back(allFiles());
    return listed;
}

} // namespace Ui::Io::Platform
