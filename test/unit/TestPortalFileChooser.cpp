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
 * @file TestPortalFileChooser.cpp
 * @brief Guard for Ui::Io::Platform::PortalFileChooser - the desktop's own file dialog through
 *        xdg-desktop-portal, asked over D-Bus and never waited on, so the window keeps answering
 *        the compositor while it is open. A stand-in portal answers on a private session bus
 *        (GTestDBus, which runs dbus-daemon), and only when a test tells it to
 */

#include <gtest/gtest.h>

#ifdef __linux__

#include "common/cfree.h"
#include "common/noncopyable.h"
#include "common/time.h"
#include "testconst.h"
#include "ui/io/filefilter.h"
#include "ui/io/platform/dbusconst.h"
#include "ui/io/platform/portalconst.h"
#include "ui/io/platform/portalfilechooser.h"
#include "ui/io/platform/portalresponse.h"

#include <algorithm>
#include <cstddef>
#include <gio/gio.h>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using PureGlUi::MOST_WAIT_MS;
using PureGlUi::POLL_STEP;
using PureGlUi::PORTAL_SERVICE_DIR;
using PureGlUi::REQUEST_NAME;

// What the chooser calls: FileChooser's OpenFile, and Close on the Request each call makes
constexpr std::string_view INTERFACES = R"XML(<node>
  <interface name="org.freedesktop.portal.FileChooser">
    <method name="OpenFile">
      <arg type="s" name="parent_window" direction="in"/>
      <arg type="s" name="title" direction="in"/>
      <arg type="a{sv}" name="options" direction="in"/>
      <arg type="o" name="handle" direction="out"/>
    </method>
  </interface>
  <interface name="org.freedesktop.portal.Request">
    <method name="Close"/>
  </interface>
</node>)XML";

using files_t   = std::optional<std::vector<std::string>>;
using variant_t = Common::c_ptr_t<GVariant, g_variant_unref>;

// The portal's FileChooser on the test's bus: it keeps what OpenFile asked and answers only when
// told, as the real one answers when the user closes its dialog
class StandInPortal final : private Common::NonCopyable {
public:
    explicit StandInPortal(const std::string & busAddress)
        : m_connection(g_dbus_connection_new_for_address_sync(
          busAddress.c_str(),
          static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT
                                            | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
          nullptr,
          nullptr,
          nullptr))
        , m_node(g_dbus_node_info_new_for_xml(INTERFACES.data(), nullptr))
    {
        // Owned before the chooser asks, so the call never finds the name free
        const variant_t owned(g_dbus_connection_call_sync(m_connection.get(),
                                                          Ui::Io::Platform::DBus::NAME,
                                                          Ui::Io::Platform::DBus::OBJECT_PATH,
                                                          Ui::Io::Platform::DBus::NAME,
                                                          REQUEST_NAME,
                                                          g_variant_new("(su)", Ui::Io::Platform::Portal::BUS_NAME, 0U),
                                                          G_VARIANT_TYPE("(u)"),
                                                          G_DBUS_CALL_FLAGS_NONE,
                                                          -1,
                                                          nullptr,
                                                          nullptr));
        m_vtable.method_call        = onMethodCall;
        m_requestVtable.method_call = onRequestCall;
        m_registration              = g_dbus_connection_register_object(
        m_connection.get(),
        Ui::Io::Platform::Portal::OBJECT_PATH,
        g_dbus_node_info_lookup_interface(m_node.get(), Ui::Io::Platform::Portal::FILE_CHOOSER),
        &m_vtable,
        this,
        nullptr,
        nullptr);
    }

    ~StandInPortal()
    {
        // A call kept waiting is answered as the portal ends, its caller long gone
        if (m_heldCall) {
            g_dbus_method_invocation_return_value(m_heldCall.release(), g_variant_new("(o)", m_requestPath.c_str()));
        }
        g_dbus_connection_unregister_object(m_connection.get(), m_requestRegistration);
        g_dbus_connection_unregister_object(m_connection.get(), m_registration);
        g_dbus_connection_close_sync(m_connection.get(), nullptr, nullptr);
    }

    // OpenFile left unanswered, as by a portal slow to start
    void setAnswerHeld(bool isHeld) { m_isAnswerHeld = isHeld; }

    [[nodiscard]] bool        isAsked() const { return m_options != nullptr; }      // an OpenFile waits for its answer
    [[nodiscard]] std::size_t askCount() const { return m_askCount; }               // OpenFile calls so far
    [[nodiscard]] const std::string & requestPath() const { return m_requestPath; } // the last call's
    [[nodiscard]] bool                isClosed() const { return m_isClosed; }       // the caller withdrew its dialog
    [[nodiscard]] const std::string & parentWindow() const { return m_parentWindow; }
    [[nodiscard]] const std::string & title() const { return m_title; }

    // An option OpenFile was given, as GVariant prints it; empty when it was not
    [[nodiscard]] std::string option(const std::string & key) const
    {
        const variant_t value(g_variant_lookup_value(m_options.get(), key.c_str(), nullptr));
        if (!value) {
            return {};
        }
        const Common::c_ptr_t<gchar, g_free> printed(g_variant_print(value.get(), TRUE));
        return printed.get();
    }

    // The dialog closing, with the files picked
    void respond(Ui::Io::Platform::PortalResponse code, const std::vector<std::string> & uris)
    {
        GVariantBuilder list;
        g_variant_builder_init(&list, G_VARIANT_TYPE_STRING_ARRAY);
        for (const std::string & uri : uris) {
            g_variant_builder_add(&list, "s", uri.c_str());
        }
        GVariantBuilder results;
        g_variant_builder_init(&results, G_VARIANT_TYPE_VARDICT);
        if (!uris.empty()) {
            g_variant_builder_add(&results, "{sv}", Ui::Io::Platform::Portal::URIS, g_variant_builder_end(&list));
        } else {
            g_variant_builder_clear(&list);
        }
        g_dbus_connection_emit_signal(m_connection.get(),
                                      nullptr,
                                      m_requestPath.c_str(),
                                      Ui::Io::Platform::Portal::REQUEST,
                                      Ui::Io::Platform::Portal::RESPONSE,
                                      g_variant_new("(ua{sv})", std::to_underlying(code), &results),
                                      nullptr);
        m_options.reset();
    }

private:
    Common::c_ptr_t<GDBusConnection, g_object_unref>       m_connection;
    Common::c_ptr_t<GDBusNodeInfo, g_dbus_node_info_unref> m_node;
    GDBusInterfaceVTable                                   m_vtable       = {};
    guint                                                  m_registration = 0;
    variant_t                                              m_options; // what the last OpenFile asked
    std::string                                            m_parentWindow;
    std::string                                            m_title;
    std::string                                            m_requestPath; // where its answer goes
    GDBusInterfaceVTable                                   m_requestVtable = {};
    guint m_requestRegistration                                            = {}; // the Request the last OpenFile made
    Common::c_ptr_t<GDBusMethodInvocation, g_object_unref> m_heldCall;           // an OpenFile not answered yet
    bool                                                   m_isAnswerHeld = false;
    bool                                                   m_isClosed     = false;
    std::size_t                                            m_askCount     = {};

    static void onMethodCall(GDBusConnection * /*connection*/,
                             const gchar * sender,
                             const gchar * /*objectPath*/,
                             const gchar * /*interfaceName*/,
                             const gchar * /*methodName*/,
                             GVariant *              parameters,
                             GDBusMethodInvocation * invocation,
                             gpointer                portalData)
    {
        auto & portal = *static_cast<StandInPortal *>(portalData);
        ++portal.m_askCount;
        const variant_t parentWindow(g_variant_get_child_value(parameters, 0));
        const variant_t title(g_variant_get_child_value(parameters, 1));
        portal.m_parentWindow = g_variant_get_string(parentWindow.get(), nullptr);
        portal.m_title        = g_variant_get_string(title.get(), nullptr);
        portal.m_options.reset(g_variant_get_child_value(parameters, 2));

        // Where the real one answers: the caller's unique name, ':' dropped and '.' made '_',
        // then the token it chose
        const variant_t token(
        g_variant_lookup_value(portal.m_options.get(), Ui::Io::Platform::Portal::HANDLE_TOKEN, G_VARIANT_TYPE_STRING));
        std::string caller = std::string(sender).substr(1);
        std::ranges::replace(caller, '.', '_');
        portal.m_requestPath = Ui::Io::Platform::Portal::REQUEST_PATH + caller + "/"
                             + (token ? g_variant_get_string(token.get(), nullptr) : "none");

        // Made before the answer, as the real one makes it, so a Close can reach it
        g_dbus_connection_unregister_object(portal.m_connection.get(), portal.m_requestRegistration);
        portal.m_requestRegistration = g_dbus_connection_register_object(
        portal.m_connection.get(),
        portal.m_requestPath.c_str(),
        g_dbus_node_info_lookup_interface(portal.m_node.get(), Ui::Io::Platform::Portal::REQUEST),
        &portal.m_requestVtable,
        &portal,
        nullptr,
        nullptr);
        if (portal.m_isAnswerHeld) {
            portal.m_heldCall.reset(invocation);
            return;
        }
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(o)", portal.m_requestPath.c_str()));
    }

    // The Request's Close, which the chooser sends to withdraw its dialog
    static void onRequestCall(GDBusConnection * /*connection*/,
                              const gchar * /*sender*/,
                              const gchar * /*objectPath*/,
                              const gchar * /*interfaceName*/,
                              const gchar * /*methodName*/,
                              GVariant * /*parameters*/,
                              GDBusMethodInvocation * invocation,
                              gpointer                portalData)
    {
        static_cast<StandInPortal *>(portalData)->m_isClosed = true;
        g_dbus_method_invocation_return_value(invocation, nullptr);
    }
};

class PortalFileChooserTest : public ::testing::Test {
protected:
    void SetUp() override { g_test_dbus_up(m_bus.get()); }

    void TearDown() override { g_test_dbus_down(m_bus.get()); }

    [[nodiscard]] std::string busAddress() const { return g_test_dbus_get_bus_address(m_bus.get()); }

    // The bus gone under every connection on it, as a session bus that dies
    void stopBus() { g_test_dbus_stop(m_bus.get()); }

    // Where the bus finds the services it starts on a call, before it is up
    void addServiceDir(const char * dir) { g_test_dbus_add_service_dir(m_bus.get(), dir); }

private:
    Common::c_ptr_t<GTestDBus, g_object_unref> m_bus { g_test_dbus_new(G_TEST_DBUS_NONE) };
};

// A bus that starts the portal on the first call, as a desktop that runs it on demand, by a
// program that never takes its name: the call waits while a stand-in takes it
class PortalFileChooserOnDemandTest : public PortalFileChooserTest {
protected:
    void SetUp() override
    {
        addServiceDir(PORTAL_SERVICE_DIR);
        PortalFileChooserTest::SetUp();
    }
};

bool hasFiles(Ui::Io::Platform::PortalFileChooser & chooser) { return chooser.takeFiles().has_value(); }

bool isAnswered(Ui::Io::Platform::PortalFileChooser & chooser, files_t & files)
{
    files = chooser.takeFiles();
    return files.has_value();
}

// Asked after as each pass of the loop asks, until the portal has the call: nothing comes before
// it answers
bool isAskedWithNothingYet(const StandInPortal & portal, Ui::Io::Platform::PortalFileChooser & chooser)
{
    const Common::Time::point_t started = Common::Time::now();
    while (!portal.isAsked() && Common::Time::msSince(started) < MOST_WAIT_MS) {
        if (hasFiles(chooser)) {
            return false;
        }
        Common::Time::sleepFor(POLL_STEP);
    }
    return portal.isAsked() && !hasFiles(chooser);
}

// The files as the loop takes them, once the portal's answer has come
files_t filesOf(Ui::Io::Platform::PortalFileChooser & chooser)
{
    files_t                     files;
    const Common::Time::point_t started = Common::Time::now();
    while (!isAnswered(chooser, files) && Common::Time::msSince(started) < MOST_WAIT_MS) {
        Common::Time::sleepFor(POLL_STEP);
    }
    return files;
}

// Asked after as the loop asks, until it says no portal answers
bool isFoundUnavailable(Ui::Io::Platform::PortalFileChooser & chooser)
{
    const Common::Time::point_t started = Common::Time::now();
    while (Common::Time::msSince(started) < MOST_WAIT_MS) {
        if (hasFiles(chooser)) {
            return false;
        }
        if (chooser.takeIsUnavailable()) {
            return true;
        }
        Common::Time::sleepFor(POLL_STEP);
    }
    return false;
}

// The context turned, as the loop turns it, until the portal is told to close the dialog: with
// no chooser asking, nothing else hands the portal its Close
bool isClosedInTime(const StandInPortal & portal)
{
    const Common::Time::point_t started = Common::Time::now();
    while (!portal.isClosed() && Common::Time::msSince(started) < MOST_WAIT_MS) {
        g_main_context_iteration(nullptr, FALSE);
        Common::Time::sleepFor(POLL_STEP);
    }
    return portal.isClosed();
}

} // namespace

// The desktop's dialog, asked for and left open: the files reach the loop once the user picks them
TEST_F(PortalFileChooserTest, ThePickedFilesArriveOnceThePortalAnswers)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("x11:2a",
                 "/start/dir",
                 "Open CAD File",
                 { Ui::Io::file_filter_t { .name = "CAD files", .spec = "*.step;*.iges" },
                   Ui::Io::file_filter_t { .name = "All files", .spec = "*" } });
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser)) << "the files came before the portal answered";
    EXPECT_EQ(portal.parentWindow(), "x11:2a");
    EXPECT_EQ(portal.title(), "Open CAD File");
    EXPECT_EQ(portal.option(Ui::Io::Platform::Portal::MULTIPLE), "true");
    EXPECT_EQ(portal.option(Ui::Io::Platform::Portal::MODAL), "true");
    EXPECT_EQ(portal.option(Ui::Io::Platform::Portal::FILTERS),
              "[('CAD files', [(uint32 0, '*.step'), (0, '*.iges')]), ('All files', [(0, '*')])]");
    EXPECT_EQ(portal.option(Ui::Io::Platform::Portal::CURRENT_FOLDER), "b'/start/dir'");

    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded,
                   { "file:///picked/one.step", "file:///picked/two%20words.iges" });
    EXPECT_EQ(filesOf(chooser), (files_t { { "/picked/one.step", "/picked/two words.iges" } }));
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Negative: a pick with no local path - a share with no mount here; in the test a file URI that
// names none, which no GIO backend asks a daemon about - is left out and logged, and the picks
// round it are still opened
TEST_F(PortalFileChooserTest, APickWithNoLocalPathIsLeftOutAndTheRestKept)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));

    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded,
                   { "file:///picked/one.step", "file:no-local-path.step", "file:///picked/two.step" });
    EXPECT_EQ(filesOf(chooser), (files_t { { "/picked/one.step", "/picked/two.step" } }));
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Negative: picks none of which has a local path give no files, as a cancel does - the log says why
TEST_F(PortalFileChooserTest, PicksWithNoLocalPathGiveNoFiles)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));

    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded, { "file:no-local-path.step" });
    EXPECT_EQ(filesOf(chooser), files_t { std::vector<std::string> {} });
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Closed with nothing picked: no files, and no other dialog to try
TEST_F(PortalFileChooserTest, ACancelledDialogGivesNoFiles)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));
    EXPECT_EQ(portal.option(Ui::Io::Platform::Portal::CURRENT_FOLDER), "") << "no folder kept, so none asked for";

    portal.respond(Ui::Io::Platform::PortalResponse::Cancelled, {});
    EXPECT_EQ(filesOf(chooser), files_t { std::vector<std::string> {} });
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Negative: a dialog ended some other way - Esc, its close button, which GTK's backend answers with
// Ended as it does a failure to show it - is the user's answer: no files, and no other dialog
TEST_F(PortalFileChooserTest, ADialogClosedAnotherWayGivesNoFiles)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));

    portal.respond(Ui::Io::Platform::PortalResponse::Ended, {});
    EXPECT_EQ(filesOf(chooser), files_t { std::vector<std::string> {} });
    EXPECT_FALSE(chooser.takeIsUnavailable()) << "an Esc on the portal's dialog opened zenity";
}

// Negative: a portal gone with its dialog up never answers, so it is reported rather than waited on
// for good, and the caller opens another
TEST_F(PortalFileChooserTest, APortalGoneWithItsDialogUpIsReported)
{
    Ui::Io::Platform::PortalFileChooser chooser;
    {
        StandInPortal portal(busAddress());
        chooser.open("", "", "Open File", {});
        ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));
    }
    EXPECT_TRUE(isFoundUnavailable(chooser));
    EXPECT_FALSE(chooser.isOpen());
}

// Negative: a session bus gone with the dialog up is reported, and the app goes on - GLib's shared
// connection ends the process when its bus closes, unless told not to
TEST_F(PortalFileChooserTest, ASessionBusGoneWithItsDialogUpIsReported)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));

    stopBus();
    EXPECT_TRUE(isFoundUnavailable(chooser));
    EXPECT_FALSE(chooser.isOpen());
}

// Negative: one dialog at a time - an open before the bus is reached, or while the dialog is up,
// asks the portal nothing more
TEST_F(PortalFileChooserTest, AnOpenWhileOneIsAskedForAsksThePortalOnce)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    chooser.open("", "", "Open Again", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));
    chooser.open("", "", "Open Once More", {});

    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded, { "file:///picked/one.step" });
    EXPECT_EQ(filesOf(chooser), (files_t { { "/picked/one.step" } }));
    EXPECT_EQ(portal.askCount(), 1U);
    EXPECT_EQ(portal.title(), "Open File");
}

// A file opened, then Ctrl+O again: the second dialog is a call of its own, answered on its own
// request with nothing left of the first
TEST_F(PortalFileChooserTest, ASecondDialogIsAnsweredOnItsOwnRequest)
{
    StandInPortal                       portal(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));
    const std::string firstRequest = portal.requestPath();
    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded, { "file:///picked/first.step" });
    ASSERT_EQ(filesOf(chooser), (files_t { { "/picked/first.step" } }));

    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser)) << "files came before the second dialog was answered";
    EXPECT_NE(portal.requestPath(), firstRequest);
    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded, { "file:///picked/second.step" });
    EXPECT_EQ(filesOf(chooser), (files_t { { "/picked/second.step" } }));
    EXPECT_EQ(portal.askCount(), 2U);
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Negative: a portal slow to answer is reported once the reply's time is up, so the caller opens
// another dialog, and the call is withdrawn, so the portal's own does not come up beside it late
TEST_F(PortalFileChooserTest, APortalSlowToAnswerIsReportedAndItsDialogWithdrawn)
{
    StandInPortal portal(busAddress());
    portal.setAnswerHeld(true);
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    EXPECT_TRUE(isFoundUnavailable(chooser));
    EXPECT_TRUE(isClosedInTime(portal));
}

// A dialog still up as the app quits is closed with it, never left on the desktop
TEST_F(PortalFileChooserTest, AChooserGoneWithItsDialogUpClosesIt)
{
    StandInPortal portal(busAddress());
    {
        Ui::Io::Platform::PortalFileChooser chooser;
        chooser.open("", "", "Open File", {});
        ASSERT_TRUE(isAskedWithNothingYet(portal, chooser));
        EXPECT_FALSE(portal.isClosed());
    }
    EXPECT_TRUE(isClosedInTime(portal));
}

// Negative: a portal replaced with its dialog up - one queued for the name takes it as the first
// leaves, both owners named in the one change, as xdg-desktop-portal --replace hands it over -
// knows nothing of the dialog, so it is reported
TEST_F(PortalFileChooserTest, APortalReplacedWithItsDialogUpIsReported)
{
    std::optional<StandInPortal> portal;
    portal.emplace(busAddress());
    const StandInPortal                 successor(busAddress());
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    ASSERT_TRUE(isAskedWithNothingYet(*portal, chooser));

    portal.reset();
    EXPECT_TRUE(isFoundUnavailable(chooser));
    EXPECT_FALSE(chooser.isOpen());
    EXPECT_FALSE(successor.isAsked());
}

// A portal not running yet is started by the call itself, which the bus holds while it comes up:
// its name taken from no one is the portal starting, never one gone, and its answer arrives
TEST_F(PortalFileChooserOnDemandTest, APortalStartedByTheCallIsNotTakenForGone)
{
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});

    // The chooser's connection, once up; turned until it has sent its call on this thread - the
    // last serial is none before - so the bus holds the call before the stand-in comes
    const Common::c_ptr_t<GDBusConnection, g_object_unref> connection(
    g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr));
    ASSERT_TRUE(connection);
    const Common::Time::point_t started = Common::Time::now();
    while (g_dbus_connection_get_last_serial(connection.get()) == guint32 {}
           && Common::Time::msSince(started) < MOST_WAIT_MS) {
        ASSERT_FALSE(hasFiles(chooser));
        Common::Time::sleepFor(POLL_STEP);
    }
    ASSERT_NE(g_dbus_connection_get_last_serial(connection.get()), guint32 {}) << "the chooser never sent its call";
    g_dbus_connection_flush_sync(connection.get(), nullptr, nullptr);

    StandInPortal portal(busAddress());
    ASSERT_TRUE(isAskedWithNothingYet(portal, chooser)) << "the held call never reached the portal";
    EXPECT_FALSE(chooser.takeIsUnavailable()) << "the portal starting was taken for one gone";
    portal.respond(Ui::Io::Platform::PortalResponse::Succeeded, { "file:///picked/one.step" });
    EXPECT_EQ(filesOf(chooser), (files_t { { "/picked/one.step" } }));
    EXPECT_FALSE(chooser.takeIsUnavailable());
}

// Negative: no session bus at all - started over ssh, under sudo, in a container - is reported,
// so the caller opens another dialog. With no display either, GLib starts no bus of its own
TEST(PortalFileChooser, ADesktopWithNoSessionBusIsReported)
{
    g_test_dbus_unset();
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    EXPECT_TRUE(isFoundUnavailable(chooser));
    EXPECT_FALSE(chooser.isOpen());
}

// Negative: a desktop with no portal is reported, so the caller opens another dialog
TEST_F(PortalFileChooserTest, ABusWithNoPortalIsReported)
{
    Ui::Io::Platform::PortalFileChooser chooser;
    chooser.open("", "", "Open File", {});
    EXPECT_TRUE(isFoundUnavailable(chooser));
}

#endif
