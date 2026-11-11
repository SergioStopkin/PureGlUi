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
 * @file TestChildProcess.cpp
 * @brief Guard for Ui::Io::Platform::ChildProcess - a program the UI thread starts and asks after, never
 *        waiting on: a Linux file dialog waited on leaves the window unable to answer the
 *        compositor's ping, and the desktop takes it for hung. A shell stands in for the dialog
 */

#include <gtest/gtest.h>

#ifdef __linux__

#include "common/system.h"
#include "common/time.h"
#include "testconst.h"
#include "ui/io/platform/childprocess.h"
#include "ui/io/platform/command.h"

#include <array>
#include <optional>
#include <string>

namespace {

using PureGlUi::MOST_WAIT_MS;
using PureGlUi::POLL_STEP;

// Asked again and again, as each pass of the loop asks, until it has ended
std::optional<std::string> resultOf(Ui::Io::Platform::ChildProcess & process)
{
    const Common::Time::point_t started = Common::Time::now();
    std::optional<std::string>  result  = process.takeResult();
    while (!result.has_value() && Common::Time::msSince(started) < MOST_WAIT_MS) {
        Common::Time::sleepFor(POLL_STEP);
        result = process.takeResult();
    }
    return result;
}

} // namespace

// A dialog open is a program running: starting it returns at once, and it is asked after
TEST(ChildProcess, AProgramStartedIsNotWaitedFor)
{
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(std::array {
    Ui::Io::Platform::command_t { .args = { "sh", "-c", "sleep 0.2; echo /picked/file" }, .environment = {} } }));
    EXPECT_TRUE(process.isRunning());
    EXPECT_FALSE(process.takeResult().has_value()) << "start waited for the program to end";
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "/picked/file\n" });
    EXPECT_FALSE(process.isRunning());
}

// Negative: a dialog closed with nothing picked is the user's answer - no output, whatever it
// wrote, and no other dialog after it
TEST(ChildProcess, ACancelledDialogGivesNothingAndNoOther)
{
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(
    std::array { Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo partial; exit 1" }, .environment = {} },
                 Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo /picked/file" }, .environment = {} } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "" });
    EXPECT_FALSE(process.isRunning());
}

// Negative: a dialog that starts but cannot run - an option it rejects, a crash - is no answer of
// the user's, so the next is started, as for one not installed
TEST(ChildProcess, ADialogThatFailsToRunGivesWayToTheNext)
{
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(
    std::array { Ui::Io::Platform::command_t { .args = { "sh", "-c", "exit 2" }, .environment = {} },
                 Ui::Io::Platform::command_t { .args = { "sh", "-c", "kill -KILL $$" }, .environment = {} },
                 Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo /picked/file" }, .environment = {} } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "/picked/file\n" });
}

// A dialog still up as the app quits is ended with it, never waited on until the user closes it
TEST(ChildProcess, AProgramStillRunningEndsWithItsOwner)
{
    const Common::Time::point_t started = Common::Time::now();
    {
        Ui::Io::Platform::ChildProcess process;
        ASSERT_TRUE(process.start(
        std::array { Ui::Io::Platform::command_t { .args = { "sh", "-c", "exec sleep 60" }, .environment = {} } }));
        ASSERT_TRUE(process.isRunning());
    }
    EXPECT_LT(Common::Time::msSince(started), MOST_WAIT_MS) << "the owner waited for the program to end on its own";
}

// A dialog started as another backend's client: what it is given is its own, over ours
TEST(ChildProcess, AProgramGetsTheEnvironmentItIsGiven)
{
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(std::array {
    Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo $GREETING" }, .environment = { "GREETING=hello" } } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "hello\n" });
}

// A variable we hold too is the program's own value, as GDK_BACKEND is given over a Wayland one
TEST(ChildProcess, AProgramGivenAVariableWeHoldGetsItsOwn)
{
    ASSERT_FALSE(Common::System::environmentVariable("HOME").empty());
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(std::array {
    Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo \"$HOME\"" }, .environment = { "HOME=/given" } } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "/given\n" });
}

// The ones it is not given are ours, so a dialog finds the display and the session it runs in
TEST(ChildProcess, AProgramKeepsTheVariablesItIsNotGiven)
{
    const std::string home = Common::System::environmentVariable("HOME");
    ASSERT_FALSE(home.empty());
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(process.start(std::array {
    Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo \"$HOME\"" }, .environment = { "GREETING=hello" } } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { home + "\n" });
}

// Negative: a variable given whose name starts with one of ours replaces nothing of ours
TEST(ChildProcess, AVariableIsNotTakenForALongerOneItStarts)
{
    const std::string home = Common::System::environmentVariable("HOME");
    ASSERT_FALSE(home.empty());
    Ui::Io::Platform::ChildProcess process;
    ASSERT_TRUE(
    process.start(std::array { Ui::Io::Platform::command_t { .args        = { "sh", "-c", "echo \"$HOME|$HOMEX\"" },
                                                             .environment = { "HOMEX=given" } } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { home + "|given\n" });
}

// Negative: a dialog not installed is not started, and the next one is
TEST(ChildProcess, AProgramMissingGivesWayToTheNext)
{
    const Ui::Io::Platform::command_t missing { .args = { "no-such-program-to-start" }, .environment = {} };
    Ui::Io::Platform::ChildProcess    process;
    EXPECT_FALSE(process.start(std::array { missing }));
    EXPECT_FALSE(process.isRunning());

    ASSERT_TRUE(process.start(
    std::array { missing,
                 Ui::Io::Platform::command_t { .args = { "sh", "-c", "echo /picked/file" }, .environment = {} } }));
    EXPECT_EQ(resultOf(process), std::optional<std::string> { "/picked/file\n" });
}

#endif
