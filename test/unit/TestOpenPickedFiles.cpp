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
 * @file TestOpenPickedFiles.cpp
 * @brief Unit tests for ui/action/openpickedfiles.h: the files an open dialog picked, each to the
 *        host's handler for its type, the last one's folder kept for the next dialog, and a type
 *        with no handler warned of rather than opened
 */

#include "common/unicode.h"
#include "testconst.h"
#include "ui/action/openpickedfiles.h"
#include "ui/res/key/iconrole.h"
#include "ui/res/type/dialog.h"
#include "ui/res/type/dialogtype.h"
#include "ui/res/type/icondefault.h"

#include <gtest/gtest.h>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

using PureGlUi::NO_HANDLER_PREFIX;
using PureGlUi::WARNING_ICON;

using opened_t = std::vector<std::pair<std::string, std::string>>; // each handler's type and file

// The host openPickedFiles is templated on, its own resManager, localeManager and windowManager:
// it records the folders kept, the files each handler was given and the dialogs opened
struct alignas(64) fake_host_t final {
    std::set<std::string>                handledTypes; // the extensions a handler is registered for
    opened_t                             opened;
    std::vector<std::string>             lastOpenDirs;
    std::vector<Ui::Res::Type::dialog_t> dialogs;
    std::string                          unknownText; // any other locale key's

    [[nodiscard]] fake_host_t &       resManager() { return *this; }
    [[nodiscard]] const fake_host_t & localeManager() const { return *this; }
    [[nodiscard]] fake_host_t &       windowManager() { return *this; }

    void setLastOpenDir(const std::string & dir) { lastOpenDirs.emplace_back(dir); }
    void openDialog(const Ui::Res::Type::dialog_t & dialog) { dialogs.emplace_back(dialog); }

    // The warning's icon for the Warning role alone, so a dialog with it asked for that role
    [[nodiscard]] Ui::Res::Type::icon_default_t iconDefault(Ui::Res::Key::IconRoleKey role) const
    {
        Ui::Res::Type::icon_default_t icon;
        if (role == Ui::Res::Key::IconRoleKey::Warning) {
            icon.icon = WARNING_ICON;
        }
        return icon;
    }

    [[nodiscard]] const std::string & get(const std::string & key) const
    {
        return key == "DialogNoHandlerContent" ? NO_HANDLER_PREFIX : unknownText;
    }

    // As Shell's: false where no handler is registered for the type
    bool runFileHandler(const std::string & extension, const std::string & file)
    {
        if (!handledTypes.contains(extension)) {
            return false;
        }
        opened.emplace_back(extension, file);
        return true;
    }
};

// A host with a handler for each of these types
fake_host_t hostHandling(std::set<std::string> types)
{
    fake_host_t host;
    host.handledTypes = std::move(types);
    return host;
}

// The one warning a pick of this type gives
void expectNoHandlerWarning(const fake_host_t & host, const std::string & typeGlob)
{
    ASSERT_EQ(host.dialogs.size(), 1U);
    const Ui::Res::Type::dialog_t & dialog = host.dialogs.front();
    EXPECT_EQ(dialog.type, Ui::Res::Type::DialogType::Warning);
    EXPECT_EQ(dialog.title, "DialogNoHandlerTitle");
    EXPECT_EQ(dialog.icon, WARNING_ICON);
    EXPECT_EQ(dialog.contentOverride, Common::Unicode::fromUtf8(NO_HANDLER_PREFIX + " " + typeGlob));
}

} // namespace

TEST(OpenPickedFiles, APickReachesTheHandlerOfItsType)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/models/part.step" });
    EXPECT_EQ(host.opened, (opened_t { { "step", "/models/part.step" } }));
    EXPECT_TRUE(host.dialogs.empty());
}

TEST(OpenPickedFiles, EveryPickReachesItsHandlerInTheOrderPicked)
{
    fake_host_t host = hostHandling({ "step", "iges" });
    Ui::Action::openPickedFiles(host, { "/models/a.step", "/models/b.iges", "/models/c.step" });
    EXPECT_EQ(host.opened,
              (opened_t { { "step", "/models/a.step" }, { "iges", "/models/b.iges" }, { "step", "/models/c.step" } }));
    EXPECT_TRUE(host.dialogs.empty());
}

// The next dialog opens where the user last picked from, once however many were picked
TEST(OpenPickedFiles, TheLastPicksFolderIsTheNextDialogs)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/first/a.step", "/second/b.step" });
    EXPECT_EQ(host.lastOpenDirs, (std::vector<std::string> { "/second" }));
}

TEST(OpenPickedFiles, APickAtTheRootKeepsTheRoot)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/part.step" });
    EXPECT_EQ(host.lastOpenDirs, (std::vector<std::string> { "/" }));
}

// A type is the handler's whatever case it is written in; the file goes to it as picked
TEST(OpenPickedFiles, ATypeIsMatchedWhateverItsCase)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/models/PART.STEP", "/models/other.Step" });
    EXPECT_EQ(host.opened, (opened_t { { "step", "/models/PART.STEP" }, { "step", "/models/other.Step" } }));
    EXPECT_TRUE(host.dialogs.empty());
}

// Spaces, non-ASCII letters and dots in a folder are the user's names, handed over untouched
TEST(OpenPickedFiles, APathIsHandedOverAsPicked)
{
    const std::string file = "/my models/v1.2/caf\xC3\xA9 part.step";
    fake_host_t       host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { file });
    EXPECT_EQ(host.opened, (opened_t { { "step", file } }));
    EXPECT_EQ(host.lastOpenDirs, (std::vector<std::string> { "/my models/v1.2" }));
}

// Negative: a dialog cancelled or closed with nothing picked changes nothing
TEST(OpenPickedFiles, NoPicksLeaveEverythingAsItWas)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, {});
    EXPECT_TRUE(host.opened.empty());
    EXPECT_TRUE(host.lastOpenDirs.empty());
    EXPECT_TRUE(host.dialogs.empty());
}

// Negative: a type no handler is registered for is warned of, never opened; the folder is still
// the next dialog's, where the user goes on looking
TEST(OpenPickedFiles, APickWithNoHandlerIsWarnedOfWithItsType)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/drawings/plan.svg" });
    EXPECT_TRUE(host.opened.empty());
    expectNoHandlerWarning(host, "*.svg");
    EXPECT_EQ(host.lastOpenDirs, (std::vector<std::string> { "/drawings" }));
}

// Negative: the standalone shell registers no handler at all, so any pick warns
TEST(OpenPickedFiles, AHostWithNoHandlersWarnsOfAnyPick)
{
    fake_host_t host;
    Ui::Action::openPickedFiles(host, { "/models/part.step" });
    EXPECT_TRUE(host.opened.empty());
    expectNoHandlerWarning(host, "*.step");
}

// Negative: the picks before one with no handler are opened, the ones after it are not
TEST(OpenPickedFiles, APickWithNoHandlerStopsThoseAfterIt)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/models/a.step", "/models/b.svg", "/models/c.step" });
    EXPECT_EQ(host.opened, (opened_t { { "step", "/models/a.step" } }));
    expectNoHandlerWarning(host, "*.svg");
}

// Negative: one warning, of the first type with no handler, however many there are
TEST(OpenPickedFiles, OnlyTheFirstTypeWithNoHandlerIsWarnedOf)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/drawings/a.svg", "/drawings/b.dxf" });
    EXPECT_TRUE(host.opened.empty());
    expectNoHandlerWarning(host, "*.svg");
}

// Negative: a file with no type has no handler, and is warned of with an empty type
TEST(OpenPickedFiles, AFileWithNoTypeIsWarnedOf)
{
    fake_host_t host = hostHandling({ "step" });
    Ui::Action::openPickedFiles(host, { "/notes/README" });
    EXPECT_TRUE(host.opened.empty());
    expectNoHandlerWarning(host, "*.");
}
