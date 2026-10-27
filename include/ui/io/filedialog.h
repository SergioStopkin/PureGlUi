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

#include "ui/io/filefilter.h"
#include "ui/window/nativewindowhandle.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifdef _WIN32
#include "common/unicode.h"

// clang-format off
#include <windows.h>
// clang-format on
// commctrl.h must precede shobjidl.h: Windows SDK 10.0.26100+ shobjidl_core.h references
// LPTBBUTTON and HIMAGELIST without including commctrl.h itself.
#include <commctrl.h>
#include <shobjidl.h>
#elifdef __APPLE__
// Workaround for Xcode 16.4 SDK regression: Icons.h in HIServices uses CALLBACK_API
// which is undefined in 64-bit C++ mode, causing "use of undeclared identifier" errors.
#define __ICONS__
#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#elifdef __linux__
#include <cstddef>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
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

    // `owner` is the window the dialog is modal to and stands above: Win32 stacks a dialog
    // by its owner, and one of none went behind that window's popups. macOS and Linux
    // stack theirs above the app themselves
    static std::vector<std::string> openFiles([[maybe_unused]] Ui::Window::NativeWindowHandle owner,
                                              const std::string &                             startDir,
                                              const std::string &                             title,
                                              const std::vector<file_filter_t> &              filters)
    {
#ifdef _WIN32
        return openFilesWin32(owner, startDir, title, filters);
#elifdef __APPLE__
        return openFilesMacOs(startDir, title, filters);
#else
        return openFilesLinux(startDir, title, filters);
#endif
    }

private:
    // Canonical spec is semicolon-separated globs ("*.step;*.iges"). Most native
    // dialogs want space-separated; macOS wants bare extensions.
    static std::string specToSpaces(const std::string & spec)
    {
        std::string out = spec;
        std::ranges::replace(out, ';', ' ');
        return out;
    }

#ifdef _WIN32
    static std::vector<std::string> openFilesWin32(HWND                               owner,
                                                   const std::string &                startDir,
                                                   const std::string &                title,
                                                   const std::vector<file_filter_t> & filters)
    {
        std::vector<std::string> files;

        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(hr)) {
            std::cerr << "[FileDialog] CoInitializeEx failed" << std::endl;
            return files;
        }

        // IID_PPV_ARGS names the interface from the pointer's type and hands COM its void**
        IFileOpenDialog * dialog = nullptr;
        hr                       = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
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
            specs.push_back({ .pszName = storage.at(i).c_str(), .pszSpec = storage.at(i + 1).c_str() });
        }
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());

        if (!startDir.empty()) {
            const std::wstring wideDir = Common::Unicode::fromUtf8(startDir);
            IShellItem *       folder  = nullptr;
            hr                         = SHCreateItemFromParsingName(wideDir.c_str(), nullptr, IID_PPV_ARGS(&folder));
            if (SUCCEEDED(hr)) {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        hr = dialog->Show(owner);
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
#elifdef __APPLE__
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

        // zenity (GNOME/GTK) and kdialog (KDE), each an argv of its own: no shell runs
        // them, so a quote in a folder or a title is one more character of its argument.
        // Both take the supplied filters plus a trailing "All files"
        std::vector<std::string> zenity = { "zenity",     "--file-selection", "--title=" + dlgTitle,
                                            "--multiple", "--separator=\n",   "--filename=" + dir + "/" };
        std::string              kdialogFilter;
        for (const auto & filter : filters) {
            const std::string patterns = specToSpaces(filter.spec);
            zenity.emplace_back("--file-filter=" + filter.name + "|" + patterns);
            kdialogFilter += patterns + "|" + filter.name + "\n";
        }
        zenity.emplace_back("--file-filter=All files|*");
        kdialogFilter += "*|All files";

        // Both support multi-selection (Ctrl+click, Shift+click)
        const std::array<std::vector<std::string>, 2> commands = {
            std::move(zenity),
            std::vector<std::string> { "kdialog", "--getopenfilename", "--multiple", dir, kdialogFilter },
        };
        for (const auto & command : commands) {
            std::string result = outputOf(command);
            if (!result.empty()) {
                return splitLines(result);
            }
        }

        std::cerr << "[FileDialog] No file dialog available (install zenity or kdialog)" << std::endl;
        return {};
    }

    // A program's stdout, run straight from its argv with no shell; empty when it is
    // missing, fails or is cancelled. Its stderr goes nowhere
    static std::string outputOf(std::vector<std::string> args)
    {
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (std::string & arg : args) {
            argv.emplace_back(arg.data());
        }
        argv.emplace_back(nullptr);

        // Both ends close on exec, so the child keeps only the copy made its stdout
        std::array<int, 2> pipeEnds {};
        if (pipe2(pipeEnds.data(), O_CLOEXEC) != 0) {
            return {};
        }
        posix_spawn_file_actions_t actions {};
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipeEnds.at(1), STDOUT_FILENO);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
        pid_t      child     = 0;
        const bool isSpawned = posix_spawnp(&child, argv.at(0), &actions, nullptr, argv.data(), environ) == 0;
        posix_spawn_file_actions_destroy(&actions);
        close(pipeEnds.at(1));

        std::string           output;
        std::array<char, 256> buffer {};
        while (isSpawned) {
            const ssize_t count = read(pipeEnds.at(0), buffer.data(), buffer.size());
            if (count <= 0) {
                break;
            }
            output.append(buffer.data(), static_cast<std::size_t>(count));
        }
        close(pipeEnds.at(0));

        // A wait status of 0 is a normal exit with code 0
        int status = 0;
        if (!isSpawned || waitpid(child, &status, 0) != child || status != 0) {
            return {};
        }
        return output;
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
