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
 * @file TestWindowIdentifier.cpp
 * @brief The main window names itself as xdg-desktop-portal takes a parent window - "x11:<XID>",
 *        or "wayland:<handle>" from xdg-foreign's export - so the desktop's file dialog is modal
 *        to it and can never fall behind it
 */

#include <gtest/gtest.h>

#if defined(HAVE_X11) || defined(HAVE_WAYLAND)

#include "shelltest.h"
#include "ui/io/platform/portalconst.h"

#include <string>

#ifdef HAVE_WAYLAND
#include "common/cfree.h"

#include <cstdint>
#include <string_view>
#include <wayland-client.h>
#else
#include "ui/io/platform/command.h"
#include "ui/io/platform/dialogcommandsof.h"

#include <algorithm>
#include <ios>
#include <sstream>
#endif

namespace {

using WindowIdentifierTest = PureGlUi::ShellTest;

#ifdef HAVE_WAYLAND

// Whether the compositor offers xdg-foreign's exporter, asked on a connection of the test's own
bool isExporterOffered()
{
    const Common::c_ptr_t<wl_display, wl_display_disconnect> display(wl_display_connect(nullptr));
    if (!display) {
        return false;
    }
    const Common::c_ptr_t<wl_registry, wl_registry_destroy> registry(wl_display_get_registry(display.get()));
    static constexpr wl_registry_listener                   LISTENER = {
        .global =
        [](void * isOffered,
           wl_registry * /*registry*/,
           uint32_t /*name*/,
           const char * interface,
           uint32_t /*version*/) {
            if (std::string_view(interface) == "zxdg_exporter_v2") {
                *static_cast<bool *>(isOffered) = true;
            }
        },
        .global_remove = [](void * /*isOffered*/, wl_registry * /*registry*/, uint32_t /*name*/) { },
    };
    bool isOffered = false;
    wl_registry_add_listener(registry.get(), &LISTENER, &isOffered);
    wl_display_roundtrip(display.get());
    return isOffered;
}

#endif

} // namespace

#ifdef HAVE_WAYLAND

// The handle the compositor exported it as, which the portal hands its dialog to parent to. A
// compositor with no exporter skips it, so a CI run there shows it was never checked
TEST_F(WindowIdentifierTest, AWaylandWindowNamesTheHandleItsCompositorExported)
{
    if (!isExporterOffered()) {
        GTEST_SKIP() << "the compositor offers no xdg-foreign exporter";
    }
    const std::string identifier = mainWindow().windowIdentifier();
    EXPECT_TRUE(identifier.starts_with(Ui::Io::Platform::Portal::WAYLAND_PREFIX)) << identifier;
    EXPECT_GT(identifier.size(), std::string_view(Ui::Io::Platform::Portal::WAYLAND_PREFIX).size()) << identifier;
}

// Negative: a compositor that exports no window leaves the dialog without a parent
TEST_F(WindowIdentifierTest, AWaylandWindowWithNoExporterNamesNothing)
{
    if (isExporterOffered()) {
        GTEST_SKIP() << "the compositor offers an xdg-foreign exporter";
    }
    EXPECT_EQ(mainWindow().windowIdentifier(), "");
}

#else

// Its id in hex, the way the portal reads an X11 parent
TEST_F(WindowIdentifierTest, AnX11WindowNamesItsIdInHex)
{
    std::ostringstream expected;
    expected << Ui::Io::Platform::Portal::X11_PREFIX << std::hex << mainWindow().nativeHandle();
    EXPECT_EQ(mainWindow().windowIdentifier(), expected.str());
}

// The fallback dialogs attach to the window the identifier names: the hex the window writes is
// read back as the decimal XID zenity and kdialog take
TEST_F(WindowIdentifierTest, AnX11IdentifierAttachesTheFallbackToThatWindow)
{
    const std::string attach = "--attach=" + std::to_string(mainWindow().nativeHandle());
    for (const Ui::Io::Platform::command_t & command :
         Ui::Io::Platform::dialogCommandsOf(mainWindow().windowIdentifier(), "", "", {})) {
        EXPECT_NE(std::ranges::find(command.args, attach), command.args.end()) << command.args.front();
    }
}

#endif

#endif
