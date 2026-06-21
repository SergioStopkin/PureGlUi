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

#include "common/unicode.h"
#include "ui/io/filedialog.h"
#include "ui/res/type/dialog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace Ui::Action {

// Lowercase file extension without the dot ("/p/Model.STEP" -> "step").
inline std::string fileExtension(const std::string & file)
{
    std::string extension = std::filesystem::path(file).extension().string();
    if (!extension.empty() && extension.front() == '.') {
        extension.erase(0, 1);
    }
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

// Warn that no handler is registered for a file type, e.g. "No handler for
// *.svg". The prefix is localized; the glob is appended at runtime. Tinted with
// --cl-warn via the Warning dialog type; icon is the data-driven "warning" role.
template <class Host>
void openNoHandlerDialog(Host & host, const std::string & extension)
{
    Ui::Res::Type::dialog_t dialog;
    dialog.type  = Ui::Res::Type::DialogType::Warning;
    dialog.icon  = host.resManager().iconDefault("warning").icon;
    dialog.title = "DialogNoHandlerTitle";

    const std::string & prefix = host.resManager().localeManager().get("DialogNoHandlerContent");
    dialog.contentOverride     = Common::Unicode::fromUtf8(prefix + " *." + extension);

    host.windowManager().openDialog(dialog);
}

// "OpenFile": native open dialog, then route each picked file to the host's
// per-extension handler (setFileHandler); a file type with no handler shows the
// warning dialog. The standalone shell registers none, so any open warns. Title
// + filters are data-driven (res app.json); empty filters => any file.
template <class Host>
void openFile(Host & host, const std::string & /*arg*/)
{
    host.closePopupMenu();
    const auto files = Ui::Io::FileDialog::openFiles(host.resManager().lastOpenDir(),
                                                     host.resManager().openFileTitle(),
                                                     host.resManager().openFileFilters());
    if (files.empty()) {
        return;
    }
    host.resManager().setLastOpenDir(std::filesystem::path(files.back()).parent_path().string());

    for (const std::string & file : files) {
        const std::string extension = fileExtension(file);
        if (!host.runFileHandler(extension, file)) {
            openNoHandlerDialog(host, extension);
            return;
        }
    }
}

} // namespace Ui::Action
