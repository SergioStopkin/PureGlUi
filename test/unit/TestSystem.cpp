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
 * @file TestSystem.cpp
 * @brief Unit tests for Common::System's environment: every variable as NAME=value, the list a
 *        program started from here is given, agreeing with each one read alone
 */

#include <gtest/gtest.h>

#ifdef __linux__

#include "common/system.h"

#include <algorithm>
#include <string>
#include <vector>

// Each a name, an '=' and its value, never a value alone
TEST(System, TheEnvironmentHoldsEveryVariableAsNameAndValue)
{
    const std::vector<std::string> variables = Common::System::environment();
    ASSERT_FALSE(variables.empty());
    for (const std::string & variable : variables) {
        EXPECT_NE(variable.find('='), std::string::npos) << variable;
        EXPECT_FALSE(variable.starts_with('=')) << variable;
    }
}

// The list and a variable read alone are the same environment
TEST(System, TheEnvironmentAgreesWithEachVariable)
{
    const std::string path = Common::System::environmentVariable("PATH");
    ASSERT_FALSE(path.empty());
    const std::vector<std::string> variables = Common::System::environment();
    EXPECT_NE(std::ranges::find(variables, "PATH=" + path), variables.end());
}

// Negative: a variable never set is in neither
TEST(System, AVariableNeverSetIsInNeither)
{
    EXPECT_EQ(Common::System::environmentVariable("PUREGLUI_NO_SUCH_VARIABLE"), "");
    const std::vector<std::string> variables = Common::System::environment();
    EXPECT_TRUE(std::ranges::none_of(variables, [](const std::string & variable) {
        return variable.starts_with("PUREGLUI_NO_SUCH_VARIABLE=");
    }));
}

#endif
