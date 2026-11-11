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

#include "common/noncopyable.h"
#include "common/system.h"
#include "ui/io/platform/command.h"

#include <algorithm>
#include <array>
#include <csignal>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <iostream>
#include <optional>
#include <span>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace Ui::Io::Platform {

/**
 * @brief A program run from its argv with no shell, asked after rather than waited on: the UI
 * thread that starts it goes on answering the compositor, which takes a window that does not for
 * hung. Its stdout is gathered as it comes, its stderr goes nowhere
 */
class ChildProcess final : private Common::NonCopyable {
public:
    ChildProcess() = default;

    // One still running is ended with it, a dialog left open as the app quits
    ~ChildProcess()
    {
        if (isRunning()) {
            // kill is POSIX's, in the signal.h <csignal> includes - a header the deprecated-headers
            // check will not let us name
            // NOLINTNEXTLINE(misc-include-cleaner)
            kill(m_child, SIGTERM);
            waitpid(m_child, nullptr, UNTIL_ENDED);
            close(m_pipe);
        }
    }

    // The first of the commands that can be started, the next where one cannot - a program not
    // installed - or where one fails to run. False where none can, or one runs already
    bool start(std::span<const command_t> commands)
    {
        if (isRunning()) {
            return false;
        }
        m_next.assign(commands.begin(), commands.end());
        return startNext();
    }

    [[nodiscard]] bool isRunning() const { return m_child != NO_CHILD; }

    // Its stdout once it has exited with 0, empty when it was cancelled or none could run; nothing
    // while it or the next after one that failed to run runs, or when none was started. Never waits
    std::optional<std::string> takeResult()
    {
        if (!isRunning()) {
            return std::nullopt;
        }
        readAvailable();
        int status {};
        // glibc defines WNOHANG in whichever of stdlib.h and sys/wait.h comes first - here stdlib.h,
        // which C++ cannot include by name: <stdlib.h> is libstdc++'s wrapper
        // NOLINTNEXTLINE(misc-include-cleaner)
        const pid_t ended = waitpid(m_child, &status, WNOHANG);
        if (ended == NO_CHILD) {
            return std::nullopt;
        }
        // What it wrote between that read and its end
        readAvailable();
        close(m_pipe);
        m_child = NO_CHILD;
        if (ended == CALL_FAILED || status != EXITED_CLEANLY) {
            m_output.clear();
            // No answer of the user's, so the next is asked for one
            if (ended != CALL_FAILED && !isAnswered(status)) {
                std::cerr << "[ChildProcess] " << m_program << " failed to run" << std::endl;
                if (startNext()) {
                    return std::nullopt;
                }
            }
            return std::string {};
        }
        return std::exchange(m_output, {});
    }

private:
    static constexpr pid_t        NO_CHILD       = 0;  // none runs, and waitpid's answer while it has not ended
    static constexpr int          NO_PIPE        = -1; // no file open
    static constexpr int          CALL_FAILED    = -1; // a POSIX call's answer when it failed
    static constexpr int          SPAWNED        = 0;  // posix_spawnp's answer when the program started
    static constexpr int          EXITED_CLEANLY = 0;  // a wait status: a normal exit with code 0
    static constexpr int          CANCELLED_EXIT = 1;  // zenity's and kdialog's exit code for a cancel
    static constexpr int          UNTIL_ENDED    = 0;  // waitpid's options to wait for the end
    static constexpr std::size_t  PIPE_ENDS      = 2;  // pipe2's pair
    static constexpr std::size_t  READ_END       = 0;
    static constexpr std::size_t  WRITE_END      = 1;
    static constexpr const char * NULL_DEVICE    = "/dev/null"; // where its stderr goes
    static constexpr mode_t       NO_MODE        = 0;           // a file opened, not made
    static constexpr nfds_t       ONE_FD         = 1;           // poll's count: the pipe alone
    static constexpr int          NO_WAIT        = 0;           // poll's timeout: only look
    static constexpr int          NONE_READY     = 0;           // poll's answer when nothing came
    static constexpr std::size_t  READ_SIZE      = 1024;        // read at a time
    static constexpr ssize_t      END_OF_FILE    = 0;           // read's answer once the program closed its end
    static constexpr char         NAME_END       = '=';         // of a NAME=value variable
    static constexpr std::size_t  ENDING_NULL    = 1;           // the null argv and envp each end in

    pid_t                 m_child = NO_CHILD; // the one running
    int                   m_pipe  = NO_PIPE;  // the read end of its stdout
    std::string           m_output;           // what it wrote so far
    std::string           m_program;          // its name, for the log
    std::deque<command_t> m_next;             // the ones still to try where it fails to run

    // The first of the ones still to try that can be started
    bool startNext()
    {
        while (!m_next.empty()) {
            const bool isSpawned = spawn(m_next.front());
            m_next.pop_front();
            if (isSpawned) {
                return true;
            }
        }
        return false;
    }

    // A pick or a cancel, the user's answer either way; any other end - a code of its own, a
    // signal - is the program failing to run. glibc defines the W macros in stdlib.h too, as WNOHANG
    [[nodiscard]] static bool isAnswered(int status)
    {
        // NOLINTNEXTLINE(misc-include-cleaner)
        const bool isExited = WIFEXITED(status);
        // NOLINTNEXTLINE(misc-include-cleaner)
        const int code = WEXITSTATUS(status);
        return isExited && (code == EXIT_SUCCESS || code == CANCELLED_EXIT);
    }

    // Started, its stdout piped to us; false where it cannot be - a program not installed
    bool spawn(const command_t & command)
    {
        if (command.args.empty()) {
            return false;
        }
        std::vector<std::string> args = command.args; // posix_spawn takes them writable
        std::vector<char *>      argv;
        argv.reserve(args.size() + ENDING_NULL);
        for (std::string & arg : args) {
            argv.emplace_back(arg.data());
        }
        argv.emplace_back(nullptr);
        std::vector<std::string> variables = environmentOver(command.environment);
        std::vector<char *>      envp;
        envp.reserve(variables.size() + ENDING_NULL);
        for (std::string & variable : variables) {
            envp.emplace_back(variable.data());
        }
        envp.emplace_back(nullptr);

        // Both ends close on exec, so the child keeps only the copy made its stdout
        std::array<int, PIPE_ENDS> pipeEnds {};
        if (pipe2(pipeEnds.data(), O_CLOEXEC) == CALL_FAILED) {
            return false;
        }
        posix_spawn_file_actions_t actions {};
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions, pipeEnds.at(WRITE_END), STDOUT_FILENO);
        posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, NULL_DEVICE, O_WRONLY, NO_MODE);
        pid_t      child     = NO_CHILD;
        const bool isSpawned = posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), envp.data())
                            == SPAWNED;
        posix_spawn_file_actions_destroy(&actions);
        close(pipeEnds.at(WRITE_END));
        if (!isSpawned) {
            close(pipeEnds.at(READ_END));
            return false;
        }
        m_child = child;
        m_pipe  = pipeEnds.at(READ_END);
        m_output.clear();
        m_program = command.args.front();
        return true;
    }

    // Ours, each a name the program is not given one for, after what it is given
    static std::vector<std::string> environmentOver(const std::vector<std::string> & given)
    {
        std::vector<std::string> variables = given;
        for (std::string & variable : Common::System::environment()) {
            // With its '=', so a name is not taken for a longer one it starts
            const std::string_view name(variable.data(), variable.find(NAME_END) + sizeof(NAME_END));
            if (std::ranges::none_of(given, [name](const std::string & own) { return own.starts_with(name); })) {
                variables.emplace_back(std::move(variable));
            }
        }
        return variables;
    }

    // Whatever has come, never waiting for more: read only what poll says is there
    void readAvailable()
    {
        pollfd                      readEnd { .fd = m_pipe, .events = POLLIN, .revents = {} };
        std::array<char, READ_SIZE> buffer {};
        while (poll(&readEnd, ONE_FD, NO_WAIT) > NONE_READY) {
            const ssize_t count = read(m_pipe, buffer.data(), buffer.size());
            if (count <= END_OF_FILE) {
                return;
            }
            m_output.append(buffer.data(), static_cast<std::size_t>(count));
        }
    }
};

} // namespace Ui::Io::Platform

#endif
