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
 * @file TestJson.cpp
 * @brief Unit tests for Common::loadJson - the domain-blind JSON file parse
 *        shared by every res loader. Covers the three outcomes: valid parse,
 *        open failure (missing file), and parse failure (malformed content).
 */

#include "common/json.h"

#include <gtest/gtest.h>

using Common::loadJson;

TEST(Json, ValidFileParses)
{
    nlohmann::json j;
    EXPECT_TRUE(loadJson(TEST_DATA_DIR "/iconstore.json", j));
    EXPECT_FALSE(j.is_null());
}

TEST(Json, MissingFileReturnsFalse)
{
    nlohmann::json j;
    EXPECT_FALSE(loadJson(TEST_DATA_DIR "/does-not-exist.json", j));
}

TEST(Json, MalformedFileReturnsFalse)
{
    nlohmann::json j;
    EXPECT_FALSE(loadJson(TEST_DATA_DIR "/malformedjson.json", j));
}
