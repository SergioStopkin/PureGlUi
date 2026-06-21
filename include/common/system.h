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

#if (defined(_WIN32) || defined(_WIN64) || defined(__WIN32__) || defined(__WINDOWS__))
#define WINDOWS 1
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#elif defined(__APPLE__)
#define APPLE 1
#elif defined(__linux__)
#define LINUX 1
#endif

#if defined(WINDOWS)
#include <windows.h>
#elif defined(APPLE)
#include <TargetConditionals.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include "common/unicode.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace Common {

// General host/system info (CPU cores + base clock, RAM, process name). OS
// primitive - depends only on the platform + Common::Unicode, never on the
// framework or any app domain.
class System final {
public:
    System()  = delete;
    ~System() = delete;

    System(const System &)             = delete;
    System & operator=(const System &) = delete;

    static unsigned int cpuCores()
    {
        const unsigned int cores = std::thread::hardware_concurrency();
        return (cores > 0) ? cores : 1;
    }

private:
    static std::string formatGHz(double ghz)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << ghz << " GHz";
        return oss.str();
    }

public:
    // Base CPU clock frequency as "X.XX GHz", or "N/A" if unavailable.
    // Apple Silicon does not expose hw.cpufrequency; we fall back to parsing
    // "@ X.YGHz" out of the CPU brand string when present.
    static std::string cpuFrequency()
    {
#ifdef __linux__
        // /proc/cpuinfo "cpu MHz" reports the instantaneous clock of the first
        // core, which idle-steps down to a few hundred MHz on modern CPUs.
        // Prefer the rated frequency embedded in "model name" (e.g.
        // "Intel(R) Core(TM) i9-...@ 3.50GHz"), falling back to sysfs
        // cpuinfo_max_freq if the brand string has no @ clause (AMD, ARM, etc.).
        std::ifstream cpuinfo("/proc/cpuinfo");
        std::string   line;
        while (std::getline(cpuinfo, line)) {
            if (line.find("model name") == 0) {
                const auto at = line.find('@');
                if (at != std::string::npos) {
                    const auto start = line.find_first_not_of(" \t", at + 1);
                    const auto ghz   = line.find("GHz", start);
                    if (start != std::string::npos && ghz != std::string::npos) {
                        return line.substr(start, ghz + 3 - start);
                    }
                }
                break;
            }
        }
        std::ifstream maxFreq("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
        long          khz = 0;
        if (maxFreq >> khz && khz > 0) {
            return formatGHz(khz / 1.0e6);
        }
#elif defined(APPLE)
        uint64_t hz  = 0;
        size_t   len = sizeof(hz);
        if (sysctlbyname("hw.cpufrequency_max", &hz, &len, nullptr, 0) == 0 && hz > 0) {
            return formatGHz(static_cast<double>(hz) / 1.0e9);
        }
        char   brand[256] = {};
        size_t brandLen   = sizeof(brand);
        if (sysctlbyname("machdep.cpu.brand_string", brand, &brandLen, nullptr, 0) == 0) {
            const std::string_view s { brand };
            const auto             at = s.find('@');
            if (at != std::string_view::npos) {
                const auto start = s.find_first_not_of(" \t", at + 1);
                const auto ghz   = s.find("GHz", start);
                if (start != std::string_view::npos && ghz != std::string_view::npos) {
                    return std::string { s.substr(start, ghz + 3 - start) };
                }
            }
        }
#endif
        return "N/A";
    }

    static std::string systemRam()
    {
#ifdef __linux__
        std::ifstream meminfo("/proc/meminfo");
        std::string   line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream iss(line);
                std::string        label;
                long               kb = 0;
                iss >> label >> kb;
                return std::to_string(kb / 1024 / 1024) + " GB";
            }
        }
#elif defined(APPLE)
        uint64_t bytes = 0;
        size_t   len   = sizeof(bytes);
        if (sysctlbyname("hw.memsize", &bytes, &len, nullptr, 0) == 0 && bytes > 0) {
            return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
        }
#endif
        return "N/A";
    }

    // Set the system/application name for the window (taskbar, dock, etc.)
    static void setSystemName(const std::string & name)
    {
        (void)name; // Unused on some platforms
#if defined(WINDOWS)
        // Windows: Set process name (shown in Task Manager) and window title (already set elsewhere)
        // No direct API to set process name, but can set AppUserModelID for taskbar grouping
        typedef HRESULT(WINAPI * SetAppUserModelIDFunc)(PCWSTR);
        HMODULE shell32 = LoadLibraryA("shell32.dll");
        if (shell32) {
            SetAppUserModelIDFunc setID = (SetAppUserModelIDFunc)GetProcAddress(
            shell32,
            "SetCurrentProcessExplicitAppUserModelID");
            if (setID) {
                setID(Common::Unicode::fromUtf8(name).c_str());
            }
            FreeLibrary(shell32);
        }
#elif defined(APPLE)
        // macOS: Set application name in the dock
        id nsApp = ((id(*)(Class, SEL))objc_msgSend)(objc_getClass("NSApplication"),
                                                     sel_registerName("sharedApplication"));
        if (nsApp) {
            id nsStr = ((id(*)(Class, SEL, const char *))objc_msgSend)(objc_getClass("NSString"),
                                                                       sel_registerName("stringWithUTF8String:"),
                                                                       name.c_str());
            ((void (*)(id, SEL, id))objc_msgSend)(nsApp, sel_registerName("setApplicationName:"), nsStr);
        }
#endif
    }
};

} // namespace Common
