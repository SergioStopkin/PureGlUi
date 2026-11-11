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

#include "common/noncopyable.h"
#include "ui/interface/iwindow.h"
#include "ui/io/filefilter.h"

#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(__APPLE__)
#include <utility>
#endif

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
#include <string_view>
#elifdef __linux__
#include "ui/io/platform/childprocess.h"
#include "ui/io/platform/command.h"
#include "ui/io/platform/dialogcommandsof.h"
#include "ui/io/platform/linesof.h"
#include "ui/io/platform/listedfiltersof.h"
#include "ui/io/platform/portalfilechooser.h"
#endif

namespace Ui::Io {

// Native open-file dialog. Domain-blind: the title and file-type filters are
// supplied by the caller (loaded from res JSON), so the framework bakes in no
// app-specific extensions. Empty filters => any file.
//
// Linux's is the desktop's own through its portal, modal to the window, else zenity or kdialog:
// each runs beside the loop, which asks after it (takeFiles) rather than waits on it, or the
// window could not answer the compositor's ping while it is open and would be taken for hung.
// Win32's and macOS's run their own loop inside open, which keeps the window answering
class FileDialog final : private Common::NonCopyable {
public:
    FileDialog() = default;

    // `owner` is the window the dialog is modal to and stands above: Win32 stacks a dialog by
    // its owner, and one of none went behind that window's popups; the portal parents its
    // dialog to the window its identifier names. macOS stacks its own above the app. One at a
    // time: an open while one is up does nothing
    void open([[maybe_unused]] const Ui::IWindow & owner,
              const std::string &                  startDir,
              const std::string &                  title,
              const std::vector<file_filter_t> &   filters)
    {
#ifdef _WIN32
        m_files = openFilesWin32(owner.nativeHandle(), startDir, title, filters);
#elifdef __APPLE__
        m_files = openFilesMacOs(startDir, title, filters);
#else
        if (isOpen()) {
            return;
        }
        const std::string                parentWindow = owner.windowIdentifier();
        const std::vector<file_filter_t> listed       = Ui::Io::Platform::listedFiltersOf(filters);
        m_commands = Ui::Io::Platform::dialogCommandsOf(parentWindow, startDir, title, listed);
        m_portal.open(parentWindow, startDir, title, listed);
#endif
    }

    // A dialog asked for and not closed yet, the portal's or a fallback's. Win32's and macOS's
    // close before open returns
    [[nodiscard]] bool isOpen() const
    {
#if defined(_WIN32) || defined(__APPLE__)
        return false;
#else
        return m_portal.isOpen() || m_process.isRunning();
#endif
    }

    // The files picked once the dialog has closed, none when it was cancelled; nothing while it
    // is open or none was. Never waits
    std::optional<std::vector<std::string>> takeFiles()
    {
#if defined(_WIN32) || defined(__APPLE__)
        return std::exchange(m_files, std::nullopt);
#else
        std::optional<std::vector<std::string>> files = m_portal.takeFiles();
        if (m_portal.takeIsUnavailable()) {
            startFallback();
        }
        if (files.has_value()) {
            return files;
        }
        std::optional<std::string> output = m_process.takeResult();
        if (!output.has_value()) {
            return std::nullopt;
        }
        return Ui::Io::Platform::linesOf(*output);
#endif
    }

private:
#if defined(_WIN32) || defined(__APPLE__)
    std::optional<std::vector<std::string>> m_files; // what a closed dialog picked, until taken
#else
    Ui::Io::Platform::PortalFileChooser        m_portal;   // the desktop's own dialog
    Ui::Io::Platform::ChildProcess             m_process;  // zenity's or kdialog's, where no portal answers
    std::array<Ui::Io::Platform::command_t, 2> m_commands; // theirs for the dialog asked for
#endif

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
    // Where no portal answers: zenity, else kdialog where zenity cannot be started or fails to
    // run - one cancelled is the user's answer
    void startFallback()
    {
        if (!m_process.start(m_commands)) {
            std::cerr << "[FileDialog] No file dialog available (install xdg-desktop-portal, zenity or kdialog)"
                      << std::endl;
        }
    }
#endif
};

} // namespace Ui::Io
