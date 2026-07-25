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
 * @file TestSession.cpp
 * @brief session.json v2 round-trip, file I/O + persist-hook behaviour, headless.
 *
 * ResManager owns the session FORMAT (serializeSession/deserializeSession) and a
 * persisted-setting registry. Pure C++ (no GL/window), so this exercises the
 * most logic-dense untested seam directly - and de-risks the Windows/macOS
 * session persistence work without a VM. ResManager is NonCopyable/NonMovable,
 * so each test constructs its instances in place. Real res is used so
 * setThemeName() resolves a valid theme and deserialize's theme reload succeeds.
 */

#include "ui/res/resmanager.h"

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

#ifndef TEST_RES_DIR
#define TEST_RES_DIR "res"
#endif

namespace {

// A serialized blob round-trips into an identical restored state.
TEST(Session, RoundTripRestoresState)
{
    Ui::Res::ResManager source { TEST_RES_DIR };
    source.loadAll();
    source.setSessionWindowGeometry(120, 240, 800, 600);
    source.setThemeName("emerald"); // a real theme in res/submenu/theme
    source.setLastOpenDir("/tmp/projects");
    source.setSessionOpenFiles({ "/tmp/a.step", "/tmp/b.step" }, "/tmp/b.step");

    const std::string blob = source.serializeSession();

    Ui::Res::ResManager restored { TEST_RES_DIR };
    restored.loadAll();
    restored.deserializeSession(blob);

    EXPECT_EQ(restored.sessionWindowX(), 120);
    EXPECT_EQ(restored.sessionWindowY(), 240);
    EXPECT_EQ(restored.sessionWindowWidth(), 800);
    EXPECT_EQ(restored.sessionWindowHeight(), 600);
    EXPECT_EQ(restored.themeName(), source.themeName());
    EXPECT_EQ(restored.lastOpenDir(), "/tmp/projects");
    EXPECT_EQ(restored.sessionActiveFile(), "/tmp/b.step");
    EXPECT_EQ(restored.sessionOpenFiles(), (std::vector<std::string> { "/tmp/a.step", "/tmp/b.step" }));
}

// The mainWindow block is written only once geometry is committed (width/height
// > 0) - zeros would mask the layout defaults on restore.
TEST(Session, GeometryOmittedUntilCommitted)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    EXPECT_EQ(manager.serializeSession().find("mainWindow"), std::string::npos);

    manager.setSessionWindowGeometry(10, 20, 30, 40);
    EXPECT_NE(manager.serializeSession().find("mainWindow"), std::string::npos);
}

// skipIfEmpty: an empty lastOpenDir is omitted; a set one is written.
TEST(Session, EmptyLastOpenDirOmitted)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    EXPECT_EQ(manager.serializeSession().find("lastOpenDir"), std::string::npos);

    manager.setLastOpenDir("/x");
    EXPECT_NE(manager.serializeSession().find("lastOpenDir"), std::string::npos);
}

// deserialize is tolerant of garbage / non-object / empty input (session.json is
// user-editable) - it must never throw or corrupt state.
TEST(Session, TolerantOfMalformedInput)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.deserializeSession("not json at all");
    manager.deserializeSession("[1, 2, 3]"); // valid JSON, not an object
    manager.deserializeSession("{}");        // object, no sections
    manager.deserializeSession("");          // empty
    SUCCEED();                               // reaching here = no throw/crash
}

// The persist hook fires on a real change but NOT on an identical set - the
// diff-guards that prevent redundant disk writes (geometry + open-files).
TEST(Session, PersistHookRespectsDiffGuards)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    int fires = 0;
    manager.setOnPersistChange([&fires] { ++fires; });

    manager.setSessionWindowGeometry(1, 2, 3, 4);
    EXPECT_EQ(fires, 1);
    manager.setSessionWindowGeometry(1, 2, 3, 4); // identical -> no fire
    EXPECT_EQ(fires, 1);
    manager.setSessionWindowGeometry(9, 2, 3, 4); // changed -> fire
    EXPECT_EQ(fires, 2);

    manager.setSessionOpenFiles({ "/a" }, "/a");
    EXPECT_EQ(fires, 3);
    manager.setSessionOpenFiles({ "/a" }, "/a"); // identical -> no fire
    EXPECT_EQ(fires, 3);
    manager.setSessionOpenFiles({ "/a", "/b" }, "/a"); // changed -> fire
    EXPECT_EQ(fires, 4);
}

// === File I/O (ResManager owns the location, so it owns the read/write) ======

// sessionPath joins the two app.json values; both are non-empty for real res.
TEST(Session, SessionPathJoinsDirAndFile)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    const std::string   path = manager.sessionPath();
    EXPECT_NE(path.find(manager.sessionDir()), std::string::npos);
    EXPECT_NE(path.find(manager.sessionFile()), std::string::npos);
}

// writeSession round-trips a blob, creates missing parent directories, and leaves
// no .tmp behind - it writes a sibling temp file and renames it over the target,
// which is what stops two racing saves interleaving into a half-written file.
TEST(Session, WriteSessionIsAtomicAndLeavesNoTemp)
{
    const std::filesystem::path dir    = std::filesystem::temp_directory_path() / "pureglui-session-test";
    const std::filesystem::path target = dir / "nested" / "session.json";
    std::filesystem::remove_all(dir);

    EXPECT_TRUE(Ui::Res::ResManager::writeSession(target.string(), R"({"version":2})"));
    ASSERT_TRUE(std::filesystem::exists(target));
    EXPECT_FALSE(std::filesystem::exists(target.string() + ".tmp"));

    std::ifstream      file(target);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    EXPECT_EQ(buffer.str(), R"({"version":2})");

    // Overwriting an existing file works and still leaves no temp.
    EXPECT_TRUE(Ui::Res::ResManager::writeSession(target.string(), "{}"));
    EXPECT_FALSE(std::filesystem::exists(target.string() + ".tmp"));

    std::filesystem::remove_all(dir);
}

// A missing session file is a fresh start, not a failure, and leaves state alone.
// Uses an explicit path: sessionPath() is relative to the CWD, so the default
// overload would pick up a real .pureglui/session.json if the app had ever run
// here - and saveSession() must never be called from a test for the same reason.
TEST(Session, LoadSessionSucceedsWhenFileAbsent)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    const std::string saved = manager.serializeSession();

    const std::filesystem::path absent = std::filesystem::temp_directory_path() / "pureglui-no-such-session.json";
    std::filesystem::remove(absent);

    EXPECT_TRUE(manager.loadSession(absent.string()));
    EXPECT_EQ(manager.serializeSession(), saved); // state untouched
}

// The full disk round-trip: writeSession out, loadSession back in.
TEST(Session, SessionRoundTripsThroughAFile)
{
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "pureglui-roundtrip" / "session.json";
    std::filesystem::remove_all(path.parent_path());

    Ui::Res::ResManager source { TEST_RES_DIR };
    source.loadAll();
    source.setSessionWindowGeometry(11, 22, 333, 444);
    source.setLastOpenDir("/models");
    ASSERT_TRUE(Ui::Res::ResManager::writeSession(path.string(), source.serializeSession()));

    Ui::Res::ResManager restored { TEST_RES_DIR };
    restored.loadAll();
    EXPECT_TRUE(restored.loadSession(path.string()));

    EXPECT_EQ(restored.sessionWindowX(), 11);
    EXPECT_EQ(restored.sessionWindowY(), 22);
    EXPECT_EQ(restored.sessionWindowWidth(), 333);
    EXPECT_EQ(restored.sessionWindowHeight(), 444);
    EXPECT_EQ(restored.lastOpenDir(), "/models");

    std::filesystem::remove_all(path.parent_path());
}

// === Restore suppression ====================================================

// Restoring drives the same setters a user edit does, so the persist hook must
// stay silent for the duration - otherwise every launch saves what it just read.
TEST(Session, PersistHookSilentDuringRestore)
{
    Ui::Res::ResManager source { TEST_RES_DIR };
    source.loadAll();
    source.setSessionWindowGeometry(5, 6, 7, 8);
    source.setLastOpenDir("/x");
    const std::string blob = source.serializeSession();

    Ui::Res::ResManager restored { TEST_RES_DIR };
    restored.loadAll();
    int fires = 0;
    restored.setOnPersistChange([&fires] { ++fires; });

    restored.deserializeSession(blob);
    EXPECT_EQ(fires, 0);

    // ...and the hook is live again afterwards.
    restored.setLastOpenDir("/y");
    EXPECT_EQ(fires, 1);
}

// isRestoringSession() is what a host checks in its own save path, because a host
// state object reached through a registry setter fires its OWN hook, which the fw
// cannot suppress. Observed from inside such a setter.
TEST(Session, IsRestoringSessionVisibleToRegistrySetters)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();

    std::string value;
    bool        seenRestoringInSetter = false;
    manager.registerPersisted(
    Ui::Res::Key::SectionKey::View,
    "hostSetting",
    [&value] { return value; },
    [&value, &seenRestoringInSetter, &manager](const std::string & restoredValue) {
        value                 = restoredValue;
        seenRestoringInSetter = manager.isRestoringSession();
    });

    EXPECT_FALSE(manager.isRestoringSession());

    value = "on";
    manager.deserializeSession(manager.serializeSession());

    EXPECT_TRUE(seenRestoringInSetter);
    EXPECT_FALSE(manager.isRestoringSession()); // cleared on the way out
}

} // namespace
