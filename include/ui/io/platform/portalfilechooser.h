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

#pragma once

#ifdef __linux__

#include "common/cfree.h"
#include "common/noncopyable.h"
#include "ui/io/filefilter.h"
#include "ui/io/platform/dbusconst.h"
#include "ui/io/platform/portalconst.h"
#include "ui/io/platform/portalresponse.h"

#include <algorithm>
#include <cstddef>
#include <gio/gio.h>
#include <glib-object.h>
#include <glib.h>
#include <initializer_list>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Ui::Io::Platform {

/**
 * @brief The desktop's own file dialog through xdg-desktop-portal's FileChooser, asked over D-Bus
 * and never waited on: the bus, the call's reply and the dialog's answer all wait in GLib's default
 * main context, which takeFiles turns once per pass of the loop while one is asked for. Modal to
 * the window its parent names
 */
class PortalFileChooser final : private Common::NonCopyable {
public:
    PortalFileChooser() = default;

    // A dialog still up as the app quits is closed with it
    ~PortalFileChooser()
    {
        if (m_connection) {
            withdraw();
            g_dbus_connection_flush_sync(m_connection.get(), nullptr, nullptr);
        }
        close();
    }

    // Asks for the dialog and returns at once: the session bus is reached and the portal asked as
    // takeFiles turns the context, and takeIsUnavailable says when either fails. The parent is the
    // portal's window identifier - "x11:<XID>", "wayland:<handle>" - empty for none. One at a
    // time: an open while one is asked for does nothing
    void open(const std::string &                parentWindow,
              const std::string &                startDir,
              const std::string &                title,
              const std::vector<file_filter_t> & filters)
    {
        if (isOpen()) {
            return;
        }
        m_isUnavailable = false;
        ++s_requestCount;
        m_token = "pureglui" + std::to_string(s_requestCount);

        GVariantBuilder options;
        g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
        addOption(Ui::Io::Platform::Portal::HANDLE_TOKEN, g_variant_new_string(m_token.c_str()), options);
        addOption(Ui::Io::Platform::Portal::MULTIPLE, g_variant_new_boolean(TRUE), options);
        addOption(Ui::Io::Platform::Portal::MODAL, g_variant_new_boolean(TRUE), options);
        addFilters(filters, options);
        // OpenFile's from xdg-desktop-portal 1.17, its FileChooser still at version 3, and GNOME's
        // dialog follows it from 45: older ones open where they like
        if (!startDir.empty()) {
            addOption(Ui::Io::Platform::Portal::CURRENT_FOLDER, g_variant_new_bytestring(startDir.c_str()), options);
        }
        m_call.reset(g_variant_ref_sink(tupleOf({ g_variant_new_string(parentWindow.c_str()),
                                                  g_variant_new_string(title.c_str()),
                                                  g_variant_builder_end(&options) })));

        m_cancellable.reset(g_cancellable_new());
        g_bus_get(G_BUS_TYPE_SESSION, m_cancellable.get(), onBus, this);
    }

    [[nodiscard]] bool isOpen() const { return m_cancellable != nullptr; }

    // Once, true: no portal dialog came of the last call - no session bus, no portal on it, none
    // with a FileChooser, none answering in time, or one or the bus gone with it up
    [[nodiscard]] bool takeIsUnavailable() { return std::exchange(m_isUnavailable, false); }

    // The files picked once the dialog has closed, none when it was cancelled; nothing while it
    // is open or none was. Never waits
    std::optional<std::vector<std::string>> takeFiles()
    {
        // Nothing else turns the context the bus, the reply and the answer wait in
        if (isOpen()) {
            while (g_main_context_iteration(nullptr, FALSE) == TRUE) { }
        }
        // A closed bus brings nothing more, and one that crashed never said the portal left
        if (m_connection && g_dbus_connection_is_closed(m_connection.get()) == TRUE) {
            closeUnavailable("the session bus closed");
        }
        return std::exchange(m_files, std::nullopt);
    }

private:
    static inline unsigned s_requestCount = 0; // each call's token its own
    static constexpr int   REPLY_TIMEOUT_MS =
    1000; // OpenFile answers before its dialog shows, about 0.1 s from a portal started cold

    Common::c_ptr_t<GCancellable, g_object_unref>
    m_cancellable; // while a dialog is asked for; ends the bus or the call still waited for
    Common::c_ptr_t<GDBusConnection, g_object_unref> m_connection; // the session bus, once reached
    Common::c_ptr_t<GVariant, g_variant_unref>       m_call;  // OpenFile's parameters, sent once the bus is reached
    std::string                                      m_token; // the call's, which names where the portal answers
    std::string                                      m_requestPath;              // where the portal answers
    guint                                            m_responseSubscription = 0; // to the dialog's answer
    guint                                            m_ownerSubscription    = 0; // to the portal leaving
    std::optional<std::vector<std::string>>          m_files; // what a closed dialog picked, until taken
    bool m_isUnavailable = false;                             // no portal dialog came of the last call, until taken

    // The bus reached: the portal answers on a path made of our name and the call's token, so the
    // answer is listened for before the call, and one that comes at once is not missed. A portal
    // gone with the dialog up never answers, so its name losing its owner is listened for too
    void ask()
    {
        std::string sender { g_dbus_connection_get_unique_name(m_connection.get()) };
        sender.erase(0, 1);
        std::ranges::replace(sender, '.', '_');
        m_requestPath          = Ui::Io::Platform::Portal::REQUEST_PATH + sender + "/" + m_token;
        m_responseSubscription = g_dbus_connection_signal_subscribe(m_connection.get(),
                                                                    Ui::Io::Platform::Portal::BUS_NAME,
                                                                    Ui::Io::Platform::Portal::REQUEST,
                                                                    Ui::Io::Platform::Portal::RESPONSE,
                                                                    m_requestPath.c_str(),
                                                                    nullptr,
                                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                                    onResponse,
                                                                    this,
                                                                    nullptr);
        m_ownerSubscription    = g_dbus_connection_signal_subscribe(m_connection.get(),
                                                                    Ui::Io::Platform::DBus::NAME,
                                                                    Ui::Io::Platform::DBus::NAME,
                                                                    Ui::Io::Platform::DBus::NAME_OWNER_CHANGED,
                                                                    Ui::Io::Platform::DBus::OBJECT_PATH,
                                                                    Ui::Io::Platform::Portal::BUS_NAME,
                                                                    G_DBUS_SIGNAL_FLAGS_NONE,
                                                                    onOwnerChanged,
                                                                    this,
                                                                    nullptr);
        g_dbus_connection_call(m_connection.get(),
                               Ui::Io::Platform::Portal::BUS_NAME,
                               Ui::Io::Platform::Portal::OBJECT_PATH,
                               Ui::Io::Platform::Portal::FILE_CHOOSER,
                               Ui::Io::Platform::Portal::OPEN_FILE,
                               m_call.get(),
                               G_VARIANT_TYPE("(o)"),
                               G_DBUS_CALL_FLAGS_NONE,
                               REPLY_TIMEOUT_MS,
                               m_cancellable.get(),
                               onReply,
                               this);
    }

    void close()
    {
        if (!isOpen()) {
            return;
        }
        if (m_connection) {
            g_dbus_connection_signal_unsubscribe(m_connection.get(), m_responseSubscription);
            g_dbus_connection_signal_unsubscribe(m_connection.get(), m_ownerSubscription);
        }
        g_cancellable_cancel(m_cancellable.get());
        m_cancellable.reset();
        m_connection.reset();
        m_call.reset();
    }

    // The portal told to close this call's dialog, shown or still to come. Flushed, as GDBus drops
    // what it has not sent once the connection goes with our hold on it
    void withdraw()
    {
        g_dbus_connection_call(m_connection.get(),
                               Ui::Io::Platform::Portal::BUS_NAME,
                               m_requestPath.c_str(),
                               Ui::Io::Platform::Portal::REQUEST,
                               Ui::Io::Platform::Portal::CLOSE,
                               nullptr,
                               nullptr,
                               G_DBUS_CALL_FLAGS_NONE,
                               -1,
                               nullptr,
                               nullptr,
                               nullptr);
        g_dbus_connection_flush(m_connection.get(), nullptr, nullptr, nullptr);
    }

    // No portal dialog comes of this call, so the caller opens another
    void closeUnavailable(std::string_view reason)
    {
        std::cerr << "[PortalFileChooser] No portal dialog: " << reason << std::endl;
        m_isUnavailable = true;
        close();
    }

    // Each filter's "*.a;*.b" as globs
    static void addFilters(const std::vector<file_filter_t> & filters, GVariantBuilder & options)
    {
        GVariantBuilder list;
        g_variant_builder_init(&list, G_VARIANT_TYPE("a(sa(us))"));
        for (const file_filter_t & filter : filters) {
            addFilter(filter.name, filter.spec, list);
        }
        addOption(Ui::Io::Platform::Portal::FILTERS, g_variant_builder_end(&list), options);
    }

    static void addFilter(const std::string & name, std::string_view spec, GVariantBuilder & list)
    {
        GVariantBuilder globs;
        g_variant_builder_init(&globs, G_VARIANT_TYPE("a(us)"));
        while (!spec.empty()) {
            const std::size_t end = spec.find(';');
            const std::string glob { spec.substr(0, end) };
            g_variant_builder_add_value(&globs,
                                        tupleOf({ g_variant_new_uint32(0), g_variant_new_string(glob.c_str()) }));
            spec = (end == std::string_view::npos) ? std::string_view {} : spec.substr(end + 1);
        }
        g_variant_builder_add_value(&list,
                                    tupleOf({ g_variant_new_string(name.c_str()), g_variant_builder_end(&globs) }));
    }

    // One a{sv} entry, the value's floating reference taken. GVariant's format-string calls are C
    // varargs, so every value here is built by its own constructor
    static void addOption(const std::string & key, GVariant * value, GVariantBuilder & options)
    {
        g_variant_builder_add_value(
        &options,
        g_variant_new_dict_entry(g_variant_new_string(key.c_str()), g_variant_new_variant(value)));
    }

    // A tuple of the values, each one's floating reference taken
    static GVariant * tupleOf(std::initializer_list<GVariant *> values)
    {
        return g_variant_new_tuple(values.begin(), values.size());
    }

    // The session bus reached, or none to reach
    static void onBus(GObject * /*source*/, GAsyncResult * result, gpointer chooserData)
    {
        GError * failure = nullptr; // written by the call, owned on the next line
        Common::c_ptr_t<GDBusConnection, g_object_unref> connection(g_bus_get_finish(result, &failure));
        const Common::c_ptr_t<GError, g_error_free>      error(failure);
        // Cancelled, the chooser it was made for may be gone
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED) == TRUE) {
            return;
        }
        auto & chooser = *static_cast<PortalFileChooser *>(chooserData);
        if (!connection) {
            chooser.closeUnavailable(error->message);
            return;
        }
        // Shared, and set by GLib to end the process once its bus closes: the dialog falls back instead
        g_dbus_connection_set_exit_on_close(connection.get(), FALSE);
        chooser.m_connection = std::move(connection);
        chooser.ask();
    }

    // The call's reply: an error says no portal answers it
    static void onReply(GObject * source, GAsyncResult * result, gpointer chooserData)
    {
        GError * failure = nullptr; // written by the call, owned on the next line
        const Common::c_ptr_t<GVariant, g_variant_unref> reply(
        g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, &failure));
        const Common::c_ptr_t<GError, g_error_free> error(failure);
        // Cancelled, the chooser it was made for may be gone
        if (reply || g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED) == TRUE) {
            return;
        }
        auto & chooser = *static_cast<PortalFileChooser *>(chooserData);
        // Timed out, the call may yet reach a portal slow to answer: D-Bus delivers the Close after
        // it, so the dialog it makes closes as it comes rather than beside the one opened instead
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_TIMED_OUT) == TRUE) {
            chooser.withdraw();
        }
        chooser.closeUnavailable(error->message);
    }

    // The portal's name changed owner: one it had is gone, and its dialog with it, whether it left or
    // was replaced by one that knows nothing of it. One taking the name from none is the portal starting
    static void onOwnerChanged(GDBusConnection * /*connection*/,
                               const gchar * /*sender*/,
                               const gchar * /*objectPath*/,
                               const gchar * /*interfaceName*/,
                               const gchar * /*signalName*/,
                               GVariant * parameters,
                               gpointer   chooserData)
    {
        const Common::c_ptr_t<GVariant, g_variant_unref> oldOwner(g_variant_get_child_value(parameters, 1));
        if (!std::string_view(g_variant_get_string(oldOwner.get(), nullptr)).empty()) {
            static_cast<PortalFileChooser *>(chooserData)->closeUnavailable("the portal left with it open");
        }
    }

    // The dialog closed: picked with the files, as URIs, or with none. Ended some other way - Esc,
    // its close button, or its backend failing to show it, which the portal answers alike - is the
    // user's close, as every toolkit takes it: an Esc must never bring up another dialog
    static void onResponse(GDBusConnection * /*connection*/,
                           const gchar * /*sender*/,
                           const gchar * /*objectPath*/,
                           const gchar * /*interfaceName*/,
                           const gchar * /*signalName*/,
                           GVariant * parameters,
                           gpointer   chooserData)
    {
        auto &                                           chooser = *static_cast<PortalFileChooser *>(chooserData);
        const Common::c_ptr_t<GVariant, g_variant_unref> response(g_variant_get_child_value(parameters, 0));
        const auto code = static_cast<Ui::Io::Platform::PortalResponse>(g_variant_get_uint32(response.get()));
        // Logged, so a backend that cannot show it leaves a trace
        if (code == Ui::Io::Platform::PortalResponse::Ended) {
            std::cerr << "[PortalFileChooser] The dialog ended without an answer" << std::endl;
        }

        const Common::c_ptr_t<GVariant, g_variant_unref> results(g_variant_get_child_value(parameters, 1));
        const Common::c_ptr_t<GVariant, g_variant_unref> uris(
        g_variant_lookup_value(results.get(), Ui::Io::Platform::Portal::URIS, G_VARIANT_TYPE_STRING_ARRAY));
        std::vector<std::string> files;
        if (code == Ui::Io::Platform::PortalResponse::Succeeded && uris) {
            gsize                                        count = 0;
            const Common::c_ptr_t<const gchar *, g_free> list(g_variant_get_strv(uris.get(), &count));
            for (const std::string uri : std::span(list.get(), count)) {
                // A share mounted through GVfs comes as its own scheme, read through its FUSE path
                const Common::c_ptr_t<GFile, g_object_unref> file(g_file_new_for_uri(uri.c_str()));
                const Common::c_ptr_t<gchar, g_free>         path(g_file_get_path(file.get()));
                if (path) {
                    files.emplace_back(path.get());
                } else {
                    std::cerr << "[PortalFileChooser] No local path for " << uri << std::endl;
                }
            }
        }
        chooser.m_files = std::move(files);
        chooser.close();
    }
};

} // namespace Ui::Io::Platform

#endif
