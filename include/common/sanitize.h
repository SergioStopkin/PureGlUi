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

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace Common {

class Sanitize final {
public:
    Sanitize()  = delete;
    ~Sanitize() = delete;

    Sanitize(const Sanitize &)             = delete;
    Sanitize & operator=(const Sanitize &) = delete;

    // Max allowed string length for locale/UI strings
    static constexpr std::size_t MAX_STRING_LENGTH = 4096;

    // Max allowed string length for URL strings
    static constexpr std::size_t MAX_URL_LENGTH = 2048;

    // Max allowed file content size (64 KB)
    static constexpr std::size_t MAX_FILE_SIZE = 65536;

    // Max allowed length for an absolute filesystem path. POSIX PATH_MAX is
    // 4096 on Linux; pick the same so the cap doesn't truncate any path the
    // OS itself would accept.
    static constexpr std::size_t MAX_PATH_LENGTH = 4096;

    // Validate and sanitize a UI string loaded from JSON.
    // Strips control characters (except newline, tab), truncates to max length.
    // Returns sanitized string. Logs a warning if the string was modified.
    static std::string string(std::string_view input, std::string_view key, std::size_t maxLength = MAX_STRING_LENGTH)
    {
        std::string result;
        result.reserve(input.size());

        // Compare through unsigned char: `char` is signed on most platforms,
        // so any UTF-8 continuation byte (0x80-0xFF) tests negative against
        // 0x20 and would be silently dropped. The cast is the standard fix
        // for signed-char vs. C0-control comparisons.
        for (const char ch : input) {
            const auto uch = static_cast<unsigned char>(ch);
            if (ch == '\n' || ch == '\t' || (uch >= 0x20 && uch != 0x7F)) {
                result += ch;
            }
        }

        if (result.size() != input.size()) {
            std::cerr << "[Sanitize] Control characters stripped from key: " << key << std::endl;
        }

        if (result.size() > maxLength) {
            std::cerr << "[Sanitize] String truncated for key: " << key << " (" << result.size() << " -> " << maxLength
                      << ")" << std::endl;
            result.resize(maxLength);
        }

        return result;
    }

    // Validate a file path from JSON - must be a plain filename within the resource directory.
    // Rejects absolute paths, path traversal (..), and non-printable characters.
    static std::string filePath(std::string_view input, std::string_view key)
    {
        if (input.empty()) {
            return {};
        }

        std::string result = string(input, key, 255);

        if (result[0] == '/' || result[0] == '\\') {
            std::cerr << "[Sanitize] Absolute path rejected for key: " << key << std::endl;
            return {};
        }

        if (result.find("..") != std::string::npos) {
            std::cerr << "[Sanitize] Path traversal rejected for key: " << key << std::endl;
            return {};
        }

        return result;
    }

    // Validate an absolute filesystem path loaded from user-editable JSON
    // (session.json model paths, lastOpenDir, etc). Differs from filePath in
    // policy, not just characters: absolute paths and ".." are allowed (legit
    // for OS paths via symlinks/mounts), but all C0 control chars - including
    // newline and tab - are stripped, embedded NUL aborts the value, and the
    // length cap is PATH_MAX-sized rather than filename-sized. Returns empty
    // on empty input or when an embedded NUL is found.
    static std::string path(std::string_view input, std::string_view key)
    {
        if (input.empty()) {
            return {};
        }
        if (input.find('\0') != std::string_view::npos) {
            std::cerr << "[Sanitize] Embedded NUL rejected for key: " << key << std::endl;
            return {};
        }

        std::string result;
        result.reserve(input.size());
        for (const char ch : input) {
            const auto uch = static_cast<unsigned char>(ch);
            if (uch >= 0x20 && uch != 0x7F) {
                result += ch;
            }
        }

        if (result.size() != input.size()) {
            std::cerr << "[Sanitize] Control characters stripped from path key: " << key << std::endl;
        }

        if (result.size() > MAX_PATH_LENGTH) {
            std::cerr << "[Sanitize] Path truncated for key: " << key << " (" << result.size() << " -> "
                      << MAX_PATH_LENGTH << ")" << std::endl;
            result.resize(MAX_PATH_LENGTH);
        }

        return result;
    }

    // Validate file content - strips control characters (except newline, tab), truncates to MAX_FILE_SIZE.
    static std::string fileContent(std::string_view input, std::string_view path)
    {
        return string(input, path, MAX_FILE_SIZE);
    }

    // Validate a URL string - must start with https://
    static std::string url(std::string_view input, std::string_view key)
    {
        std::string result = string(input, key, MAX_URL_LENGTH);

        if (!result.starts_with("https://")) {
            std::cerr << "[Sanitize] Invalid URL for key: " << key << std::endl;
            return {};
        }

        return result;
    }
};

} // namespace Common
