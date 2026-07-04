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
#include "ui/io/filefilter.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
// clang-format on
// commctrl.h must precede shobjidl.h: Windows SDK 10.0.26100+ shobjidl_core.h references
// LPTBBUTTON and HIMAGELIST without including commctrl.h itself.
#include <commctrl.h>
#include <shobjidl.h>
#elif defined(__APPLE__)
// Workaround for Xcode 16.4 SDK regression: Icons.h in HIServices uses CALLBACK_API
// which is undefined in 64-bit C++ mode, causing "use of undeclared identifier" errors.
#define __ICONS__
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#elif defined(__linux__)
#include <cstdio>
#include <memory>
#endif

namespace Ui::Io {

// Native open-file dialog. Domain-blind: the title and file-type filters are
// supplied by the caller (loaded from res JSON), so the framework bakes in no
// app-specific extensions. Empty filters => any file.
class FileDialog final {
public:
    FileDialog()  = delete;
    ~FileDialog() = delete;

    FileDialog(const FileDialog &)             = delete;
    FileDialog(FileDialog &&)                  = delete;
    FileDialog & operator=(const FileDialog &) = delete;
    FileDialog & operator=(FileDialog &&)      = delete;

    static std::vector<std::string> openFiles(const std::string &                startDir,
                                              const std::string &                title,
                                              const std::vector<file_filter_t> & filters)
    {
#if defined(_WIN32)
        return openFilesWin32(startDir, title, filters);
#elif defined(__APPLE__)
        return openFilesMacOs(startDir, title, filters);
#else
        return openFilesLinux(startDir, title, filters);
#endif
    }

private:
#if defined(_WIN32)
    // COM out-parameter: interfaces are returned through void**. Project rule:
    // no reinterpret_cast - the two-step static_cast through void* is the same
    // defined operation, and this named helper is its single home.
    template <typename T>
    static void ** comOut(T ** interfacePointer)
    {
        return static_cast<void **>(static_cast<void *>(interfacePointer));
    }
#endif

    // Canonical spec is semicolon-separated globs ("*.step;*.iges"). Most native
    // dialogs want space-separated; macOS wants bare extensions.
    static std::string specToSpaces(const std::string & spec)
    {
        std::string out = spec;
        std::replace(out.begin(), out.end(), ';', ' ');
        return out;
    }

#if defined(_WIN32)
    static std::vector<std::string> openFilesWin32(const std::string &                startDir,
                                                   const std::string &                title,
                                                   const std::vector<file_filter_t> & filters)
    {
        std::vector<std::string> files;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) {
            std::cerr << "[FileDialog] CoInitializeEx failed" << std::endl;
            return files;
        }

        IFileOpenDialog * dialog = nullptr;
        hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, comOut(&dialog));
        if (FAILED(hr)) {
            CoUninitialize();
            return files;
        }

        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_ALLOWMULTISELECT | FOS_FILEMUSTEXIST);

        if (!title.empty()) {
            dialog->SetTitle(Common::Unicode::fromUtf8(title).c_str());
        }

        // Build COMDLG_FILTERSPEC from the supplied filters (wide strings must
        // outlive the SetFileTypes call). Fall back to "All files".
        std::vector<std::wstring>      storage;
        std::vector<COMDLG_FILTERSPEC> specs;
        for (const auto & filter : filters) {
            storage.emplace_back(Common::Unicode::fromUtf8(filter.name));
            storage.emplace_back(Common::Unicode::fromUtf8(filter.spec));
        }
        storage.emplace_back(L"All files");
        storage.emplace_back(L"*.*");
        for (size_t i = 0; i + 1 < storage.size(); i += 2) {
            specs.push_back({ storage[i].c_str(), storage[i + 1].c_str() });
        }
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());

        if (!startDir.empty()) {
            const std::wstring wideDir = Common::Unicode::fromUtf8(startDir);
            IShellItem *       folder  = nullptr;
            hr = SHCreateItemFromParsingName(wideDir.c_str(), nullptr, IID_IShellItem, comOut(&folder));
            if (SUCCEEDED(hr)) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        hr = dialog->Show(nullptr);
        if (SUCCEEDED(hr)) {
            IShellItemArray * results = nullptr;
            hr                        = dialog->GetResults(&results);
            if (SUCCEEDED(hr)) {
                DWORD count = 0;
                results->GetCount(&count);
                for (DWORD i = 0; i < count; ++i) {
                    IShellItem * item = nullptr;
                    if (SUCCEEDED(results->GetItemAt(i, &item))) {
                        PWSTR path = nullptr;
                        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                            files.emplace_back(Common::Unicode::toUtf8(path));
                            CoTaskMemFree(path);
                        }
                        item->Release();
                    }
                }
                results->Release();
            }
        }

        dialog->Release();
        CoUninitialize();
        return files;
    }
#elif defined(__APPLE__)
    static std::vector<std::string> openFilesMacOs(const std::string &                startDir,
                                                   const std::string &                title,
                                                   const std::vector<file_filter_t> & filters)
    {
        std::vector<std::string> files;
        @autoreleasepool {
            NSOpenPanel * panel = [NSOpenPanel openPanel];
            [panel setAllowsMultipleSelection:YES];
            [panel setCanChooseDirectories:NO];
            [panel setCanChooseFiles:YES];
            if (!title.empty()) {
                [panel setTitle:[NSString stringWithUTF8String:title.c_str()]];
            }

            // Bare extensions from each filter's "*.ext;*.ext2" spec.
            NSMutableArray<UTType *> * types = [NSMutableArray array];
            for (const auto & filter : filters) {
                std::string_view sv(filter.spec);
                while (!sv.empty()) {
                    const auto       pos  = sv.find(';');
                    std::string_view glob = sv.substr(0, pos);
                    const auto       dot  = glob.rfind('.');
                    if (dot != std::string_view::npos) {
                        std::string ext { glob.substr(dot + 1) };
                        UTType *    t = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:ext.c_str()]];
                        if (t) {
                            [types addObject:t];
                        }
                    }
                    sv = (pos == std::string_view::npos) ? std::string_view {} : sv.substr(pos + 1);
                }
            }
            if ([types count] > 0) {
                [panel setAllowedContentTypes:types];
            }

            if (!startDir.empty()) {
                NSString * dir = [NSString stringWithUTF8String:startDir.c_str()];
                [panel setDirectoryURL:[NSURL fileURLWithPath:dir]];
            }

            if ([panel runModal] != NSModalResponseOK) {
                return files;
            }

            for (NSURL * url in [panel URLs]) {
                const char * path = [[url path] UTF8String];
                if (path != nullptr) {
                    files.emplace_back(path);
                }
            }
        }
        return files;
    }
#else
    static std::vector<std::string> openFilesLinux(const std::string &                startDir,
                                                   const std::string &                title,
                                                   const std::vector<file_filter_t> & filters)
    {
        const std::string dir      = startDir.empty() ? "." : startDir;
        const std::string dlgTitle = title.empty() ? "Open File" : title;

        // zenity (GNOME/GTK) filter args + kdialog (KDE) filter string, both
        // derived from the supplied filters; a trailing "All files" is appended.
        std::string zenityFilters;
        std::string kdialogFilter;
        for (const auto & filter : filters) {
            const std::string patterns = specToSpaces(filter.spec);
            zenityFilters += " --file-filter='" + filter.name + "|" + patterns + "'";
            kdialogFilter += patterns + "|" + filter.name + "\n";
        }
        zenityFilters += " --file-filter='All files|*'";
        kdialogFilter += "*|All files";

        // Both support multi-selection (Ctrl+click, Shift+click).
        const std::array<std::string, 2> commands = {
            "zenity --file-selection --title='" + dlgTitle + "' --multiple --separator='\n' --filename='" + dir + "/'"
            + zenityFilters + " 2>/dev/null",

            "kdialog --getopenfilename --multiple '" + dir + "' '" + kdialogFilter + "' 2>/dev/null"
        };

        for (const auto & command : commands) {
            // NOLINTNEXTLINE(cert-env33-c)
            std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
            if (!pipe) {
                continue;
            }

            std::string           result;
            std::array<char, 256> buffer {};
            while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) != nullptr) {
                result += buffer.data();
            }

            const int status = pclose(pipe.release());
            if (status == 0 && !result.empty()) {
                return splitLines(result);
            }
        }

        std::cerr << "[FileDialog] No file dialog available (install zenity or kdialog)" << std::endl;
        return {};
    }
#endif

    static std::vector<std::string> splitLines(std::string & text)
    {
        if (!text.empty() && text.back() == '\n') {
            text.pop_back();
        }
        std::vector<std::string> files;
        std::string_view         sv(text);
        while (!sv.empty()) {
            const auto pos = sv.find('\n');
            files.emplace_back(sv.substr(0, pos));
            sv = (pos == std::string_view::npos) ? std::string_view {} : sv.substr(pos + 1);
        }
        return files;
    }
};

} // namespace Ui::Io
