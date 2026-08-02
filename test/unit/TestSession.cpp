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

#include "nlohmann/json.hpp"
#include "ui/res/key/section.h"
#include "ui/res/key/session.h"
#include "ui/res/resmanager.h"

#include <cstdint>
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

// Half-committed geometry is still not geometry: the guard is width AND height,
// so a window that reported a width before its height must not persist a zero
// dimension that restore would take literally.
TEST(Session, PartialGeometryIsStillOmitted)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();

    manager.setSessionWindowGeometry(10, 20, 30, 0);
    EXPECT_EQ(manager.serializeSession().find("mainWindow"), std::string::npos);

    manager.setSessionWindowGeometry(10, 20, 0, 40);
    EXPECT_EQ(manager.serializeSession().find("mainWindow"), std::string::npos);
}

// The files block is absent entirely on a fresh profile, and appears as soon as
// either half has something - active alone is enough (a file opened and closed
// still names the last active path).
TEST(Session, FilesSectionOmittedUntilThereIsSomethingToSay)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();

    const nlohmann::json fresh = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_FALSE(fresh.contains("files"));

    manager.setSessionOpenFiles({}, "/a.step");
    const nlohmann::json activeOnly = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(activeOnly["files"].is_object());
    EXPECT_EQ(activeOnly["files"]["active"], "/a.step");
    EXPECT_TRUE(activeOnly["files"]["open"].is_array());
    EXPECT_TRUE(activeOnly["files"]["open"].empty());
}

// The files block is written key by key, so a host key registered into the same
// section survives. A whole-node assignment would drop it silently, and only
// once a file is open - the block is skipped entirely on an empty profile, which
// is what would make that regression look intermittent.
TEST(Session, FilesSectionKeepsAHostKeyRegisteredBesideIt)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersisted(
    Ui::Res::Key::SectionKey::Files,
    "lastExported",
    [] { return std::string { "/tmp/out.step" }; },
    [](const std::string &) {});
    manager.setSessionOpenFiles({ "/a.step" }, "/a.step");

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(json["files"].is_object());
    EXPECT_EQ(json["files"]["lastExported"], "/tmp/out.step"); // the host key
    EXPECT_EQ(json["files"]["active"], "/a.step");             // and both fw keys
    EXPECT_EQ(json["files"]["open"], (std::vector<std::string> { "/a.step" }));
}

// No dock state, no docks array - an empty collection section is noise that
// every later read has to skip past anyway.
TEST(Session, DocksSectionOmittedWhenEmpty)
{
    Ui::Res::ResManager  manager { TEST_RES_DIR };
    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_FALSE(json.contains("docks"));
}

// version + lastUpdate are the only unconditional writes: they identify the
// format, so their absence is a silent break rather than a missing setting.
TEST(Session, RootStampsAreAlwaysWritten)
{
    Ui::Res::ResManager  manager { TEST_RES_DIR };
    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(json.is_object());
    EXPECT_TRUE(json.contains("version"));
    ASSERT_TRUE(json["lastUpdate"].is_number());
    EXPECT_GT(json["lastUpdate"].get<int64_t>(), 0);
}

// A path is bytes, not text: a filename that is not valid UTF-8 is legal on
// Linux and reaches the blob verbatim. Default dump() throws on it, which would
// escape saveSession and cost the user every persisted setting, so the writer
// substitutes instead of failing.
TEST(Session, NonUtf8PathDoesNotBreakTheWholeSave)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.setLastOpenDir("/x");
    manager.setSessionOpenFiles({ "/models/file\xff"
                                  ".step" },
                                "/models/file\xff"
                                ".step");

    std::string blob;
    ASSERT_NO_THROW(blob = manager.serializeSession());

    const nlohmann::json json = nlohmann::json::parse(blob, nullptr, false);
    ASSERT_TRUE(json.is_object());
    EXPECT_EQ(json["paths"]["lastOpenDir"], "/x"); // everything else still saved
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

    // Scoped: on Windows an open read handle blocks the rename that replaces the
    // target, so leaving this stream open would fail the overwrite below (and the
    // cleanup) for a reason that has nothing to do with the code under test.
    {
        std::ifstream      file(target);
        std::ostringstream buffer;
        buffer << file.rdbuf();
        EXPECT_EQ(buffer.str(), R"({"version":2})");
    }

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
    // lastUpdate is stamped as the blob is built, so two calls a millisecond apart
    // differ there by design. Drop it and compare everything that is real state.
    auto withoutTimestamp = [](const std::string & blob) {
        nlohmann::json json = nlohmann::json::parse(blob);
        json.erase(Ui::Res::Key::sessionKeyName(Ui::Res::Key::SessionKey::LastUpdate));
        return json;
    };
    EXPECT_EQ(withoutTimestamp(manager.serializeSession()), withoutTimestamp(saved)); // state untouched
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

// ============================================================================
// Host-owned sections (registerPersistedJson)
//
// A host brings its own closed set plus its own sectionKeyName overload, which
// the framework finds by ADL. It stores only the resolved name, so these cases
// are about that boundary: what a host may claim, what it may not, and what
// happens to malformed content.
// ============================================================================

enum class HostSection : unsigned char {
    Extras,     // a section of the host's own
    Colliding,  // deliberately spelled like a framework section carrying keys
    Typed,      // deliberately spelled like a framework typed block
    Unmappable, // no case in the mapper below - resolves to an empty name
};

[[nodiscard]] inline std::string sectionKeyName(HostSection section)
{
    switch (section) {
    case HostSection::Extras: return "extras";
    case HostSection::Colliding: return "paths";
    case HostSection::Typed: return "files";
    case HostSection::Unmappable: break;
    }
    return {};
}

// Named rather than inline: `!requires(...) {...}` parses as a requires-CLAUSE,
// which evaluates the body for real and turns a negative check into a compile
// error instead of a false.
template <typename Section>
concept RegistersAsJsonSection = requires(
Ui::Res::ResManager & manager,
Section               section,
Ui::provider_fn_t     getter,
Ui::action_fn_t       setter) { manager.registerPersistedJson(section, getter, setter); };

// A host cannot claim a framework section by TYPE, and cannot pass a bare
// string either - both are compile errors, not runtime checks.
static_assert(RegistersAsJsonSection<HostSection>);
static_assert(!RegistersAsJsonSection<Ui::Res::Key::SectionKey>);
static_assert(!RegistersAsJsonSection<std::string>);

TEST(Session, HostSectionLandsAsRealJson)
{
    // Real JSON under the host's own name, not an escaped string - the whole
    // reason this API exists alongside the string registry.
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { R"([{"kept":1}])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(json.is_object());
    ASSERT_TRUE(json["extras"].is_array());
    EXPECT_EQ(json["extras"][0]["kept"], 1);
}

TEST(Session, HostSectionRoundTripsBackToItsSetter)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { R"(["a","b"])" }; },
    [](const std::string &) {});

    const std::string blob = manager.serializeSession();

    std::string         seen;
    Ui::Res::ResManager restored { TEST_RES_DIR };
    restored.loadAll();
    restored.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string {}; },
    [&seen](const std::string & value) { seen = value; });
    restored.deserializeSession(blob);

    EXPECT_EQ(nlohmann::json::parse(seen, nullptr, false), nlohmann::json::parse(R"(["a","b"])", nullptr, false));
}

TEST(Session, HostSectionCoexistsWithFrameworkSections)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.setLastOpenDir("/x");
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { R"(["own"])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_TRUE(json["extras"].is_array());
    EXPECT_EQ(json["paths"]["lastOpenDir"], "/x");
    EXPECT_TRUE(json.contains("view")); // framework keys untouched
}

// --- negative --------------------------------------------------------------

TEST(Session, HostSectionNamedLikeAKeyedFrameworkSectionIsRefused)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.setLastOpenDir("/x");
    manager.registerPersistedJson(
    HostSection::Colliding,
    [] { return std::string { R"(["stolen"])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(json["paths"].is_object()); // still the framework's
    EXPECT_EQ(json["paths"]["lastOpenDir"], "/x");
}

TEST(Session, HostSectionNamedLikeAFrameworkTypedBlockIsRefused)
{
    // "files" carries no registry keys, so only the framework-name check stops
    // this - and it must stop it even when the file list is EMPTY and the
    // framework would not have written the section at all this save.
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Typed,
    [] { return std::string { R"(["stolen"])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_FALSE(json.contains("files"));
}

TEST(Session, HostSectionWithAnEmptyNameIsRefused)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Unmappable,
    [] { return std::string { R"(["nowhere"])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_FALSE(json.contains("")); // no section written under an empty key
}

TEST(Session, SecondProviderForTheSameSectionIsRefused)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { R"(["first"])" }; },
    [](const std::string &) {});
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { R"(["second"])" }; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    ASSERT_TRUE(json["extras"].is_array());
    EXPECT_EQ(json["extras"][0], "first"); // first registration keeps the section
}

TEST(Session, UnparseableProviderTextCostsOnlyItsOwnSection)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.setLastOpenDir("/x");
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string { "{{{ not json" }; },
    [](const std::string &) {});

    std::string blob;
    ASSERT_NO_THROW(blob = manager.serializeSession());

    const nlohmann::json json = nlohmann::json::parse(blob, nullptr, false);
    EXPECT_FALSE(json.contains("extras"));
    EXPECT_EQ(json["paths"]["lastOpenDir"], "/x"); // the rest of the save survives
}

TEST(Session, EmptyProviderTextOmitsTheSection)
{
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string {}; },
    [](const std::string &) {});

    const nlohmann::json json = nlohmann::json::parse(manager.serializeSession(), nullptr, false);
    EXPECT_FALSE(json.contains("extras"));
}

TEST(Session, AbsentSectionLeavesTheHostSetterUntouched)
{
    // A blob without the section must not call the setter: the host would read
    // that as "restore to empty" and wipe state it should have kept.
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();

    bool wasCalled = false;
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string {}; },
    [&wasCalled](const std::string &) { wasCalled = true; });
    manager.deserializeSession(R"({"version":"1.0"})");

    EXPECT_FALSE(wasCalled);
}

TEST(Session, HostSectionOfTheWrongJsonTypeStillReachesTheSetter)
{
    // The framework does not police the shape - a host that wrote an array and
    // finds an object is the only one able to decide what that means.
    Ui::Res::ResManager manager { TEST_RES_DIR };
    manager.loadAll();

    std::string seen;
    manager.registerPersistedJson(
    HostSection::Extras,
    [] { return std::string {}; },
    [&seen](const std::string & value) { seen = value; });
    manager.deserializeSession(R"({"extras":{"unexpected":true}})");

    EXPECT_EQ(seen, R"({"unexpected":true})");
}

} // namespace
