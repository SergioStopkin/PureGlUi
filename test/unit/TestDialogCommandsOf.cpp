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
 * @file TestDialogCommandsOf.cpp
 * @brief Guard for Ui::Io::Platform::dialogCommandsOf - zenity's and kdialog's command, where no portal
 *        answers: under an X11 parent each is attached to it as an X11 client, so it stays above
 *        the window and keeps it from the focus, as the portal's dialog does
 */

#include <gtest/gtest.h>

#ifdef __linux__

#include "ui/io/filefilter.h"
#include "ui/io/platform/command.h"
#include "ui/io/platform/dialogcommandsof.h"

#include <algorithm>
#include <string>
#include <vector>

namespace {

bool hasArg(const Ui::Io::Platform::command_t & command, const std::string & arg)
{
    return std::ranges::find(command.args, arg) != command.args.end();
}

bool isAttached(const Ui::Io::Platform::command_t & command)
{
    return std::ranges::any_of(command.args, [](const std::string & arg) { return arg.starts_with("--attach"); });
}

} // namespace

// Attached to the window by its id, as clients of the X server it stands on even on a Wayland
// desktop, there being no other way to reach it
TEST(DialogCommandsOf, AnX11ParentAttachesBothDialogs)
{
    const auto                          commands = Ui::Io::Platform::dialogCommandsOf("x11:1a2b", "/start", "Open", {});
    const Ui::Io::Platform::command_t & zenity   = commands.at(0);
    const Ui::Io::Platform::command_t & kdialog  = commands.at(1);
    EXPECT_EQ(zenity.args.at(0), "zenity");
    EXPECT_TRUE(hasArg(zenity, "--modal"));
    EXPECT_TRUE(hasArg(zenity, "--attach=6699"));
    EXPECT_EQ(zenity.environment, std::vector<std::string> { "GDK_BACKEND=x11" });
    EXPECT_EQ(kdialog.args.at(0), "kdialog");
    EXPECT_TRUE(hasArg(kdialog, "--attach=6699"));
    EXPECT_EQ(kdialog.environment, std::vector<std::string> { "QT_QPA_PLATFORM=xcb" });
}

// Negative: a Wayland parent or none is nothing either can attach to, so each runs as it would
TEST(DialogCommandsOf, AWaylandParentOrNoneAttachesNeither)
{
    for (const std::string & parent : std::vector<std::string> { "wayland:abc", "" }) {
        for (const Ui::Io::Platform::command_t & command :
             Ui::Io::Platform::dialogCommandsOf(parent, "/start", "Open", {})) {
            EXPECT_FALSE(isAttached(command)) << parent;
            EXPECT_FALSE(hasArg(command, "--modal")) << parent;
            EXPECT_TRUE(command.environment.empty()) << parent;
        }
    }
}

// Both take the title, the folder and the filters in their order
TEST(DialogCommandsOf, BothTakeTheTitleTheFolderAndTheFilters)
{
    const auto commands = Ui::Io::Platform::dialogCommandsOf(
    "",
    "/start",
    "Open CAD File",
    { Ui::Io::file_filter_t { .name = "CAD files", .spec = "*.step;*.iges" },
      Ui::Io::file_filter_t { .name = "All files", .spec = "*" } });
    EXPECT_EQ(commands.at(0).args,
              (std::vector<std::string> { "zenity",
                                          "--file-selection",
                                          "--title=Open CAD File",
                                          "--multiple",
                                          "--separator=\n",
                                          "--filename=/start/",
                                          "--file-filter=CAD files|*.step *.iges",
                                          "--file-filter=All files|*" }));
    EXPECT_EQ(commands.at(1).args,
              (std::vector<std::string> { "kdialog",
                                          "--title",
                                          "Open CAD File",
                                          "--getopenfilename",
                                          "--multiple",
                                          "/start",
                                          "*.step *.iges|CAD files\n*|All files" }));
}

// Negative: the first open, with no folder kept and no title, opens where the app runs, under a
// title of its own
TEST(DialogCommandsOf, AnEmptyFolderAndTitleFallBackToTheirDefaults)
{
    const auto commands = Ui::Io::Platform::dialogCommandsOf("", "", "", {});
    EXPECT_TRUE(hasArg(commands.at(0), "--title=Open File"));
    EXPECT_TRUE(hasArg(commands.at(0), "--filename=./"));
    EXPECT_EQ(
    commands.at(1).args,
    (std::vector<std::string> { "kdialog", "--title", "Open File", "--getopenfilename", "--multiple", ".", "" }));
}

#endif
