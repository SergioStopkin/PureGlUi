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
 * @file TestFs.cpp
 * @brief Unit tests for Common::dirExists - guards the directory_iterator loops
 *        in the res loaders. Covers an existing directory, a missing path, and a
 *        path that exists but is a file (not a directory).
 */

#include "common/fs.h"

#include <gtest/gtest.h>

using Common::dirExists;

TEST(Fs, ExistingDirectoryIsTrue) { EXPECT_TRUE(dirExists(TEST_DATA_DIR)); }

TEST(Fs, MissingPathIsFalse) { EXPECT_FALSE(dirExists(TEST_DATA_DIR "/no-such-directory")); }

TEST(Fs, FilePathIsNotADirectory)
{
    // Exists, but it is a file - dirExists must reject it.
    EXPECT_FALSE(dirExists(TEST_DATA_DIR "/iconstore.json"));
}
