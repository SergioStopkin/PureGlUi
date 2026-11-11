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

/**
 * @file TestListedFiltersOf.cpp
 * @brief Unit tests for ui/io/platform/listedfiltersof.h: what a Linux open dialog lists - the
 *        caller's filters in their order, then any file, added in this one place
 */

#include "ui/io/filefilter.h"
#include "ui/io/platform/listedfiltersof.h"

#include <gtest/gtest.h>
#include <vector>

using Ui::Io::file_filter_t;

// The caller's filters keep their order, and any file comes after them, once
TEST(ListedFiltersOf, AnyFileComesLastAfterTheCallersFilters)
{
    const file_filter_t cad { .name = "CAD files", .spec = "*.step;*.iges" };
    const file_filter_t mesh { .name = "Mesh files", .spec = "*.stl" };
    EXPECT_EQ(Ui::Io::Platform::listedFiltersOf({ cad, mesh }),
              (std::vector<file_filter_t> { cad, mesh, Ui::Io::Platform::allFiles() }));
}

TEST(ListedFiltersOf, AnyFileIsEveryFile)
{
    EXPECT_EQ(Ui::Io::Platform::allFiles().spec, "*");
    EXPECT_EQ(Ui::Io::Platform::allFiles().name, "All files");
}

// Negative: no filters from res lists any file alone, so the dialog still opens on everything
TEST(ListedFiltersOf, NoFiltersListAnyFileAlone)
{
    EXPECT_EQ(Ui::Io::Platform::listedFiltersOf({}), (std::vector<file_filter_t> { Ui::Io::Platform::allFiles() }));
}
