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

#include "common/sanitize.h"
#include "common/unicode.h"
#include "ui/res/localemanager.h"
#include "ui/res/type/dialog.h"

#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <string>

namespace Ui::Window {

// `dialog` with what it shows filled in (contentOverride): its file's text, cut at the
// sanitizer's limit, else its locale content with the host's placeholders resolved by
// `resolver` - shown verbatim where the host gave none
[[nodiscard]] inline Ui::Res::Type::dialog_t
resolvedDialogOf(const Ui::Res::Type::dialog_t &                          dialog,
                 const Ui::Res::LocaleManager &                           locale,
                 const std::function<std::wstring(const std::string &)> & resolver)
{
    Ui::Res::Type::dialog_t resolved = dialog;
    if (!resolved.file.empty()) {
        std::ifstream file(resolved.file, std::ios::binary);
        if (file.is_open()) {
            const std::string  bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            const std::wstring content = Common::Unicode::fromUtf8(bytes);
            if (content.size() > Common::Sanitize::MAX_FILE_SIZE) {
                std::cerr << "[Sanitize] File content truncated: " << resolved.file << std::endl;
                resolved.contentOverride = content.substr(0, Common::Sanitize::MAX_FILE_SIZE);
            } else {
                resolved.contentOverride = content;
            }
        }
    } else if (!resolved.content.empty()) {
        const std::string & content = locale.get(resolved.content);
        resolved.contentOverride    = resolver ? resolver(content) : Common::Unicode::fromUtf8(content);
    }
    return resolved;
}

} // namespace Ui::Window
