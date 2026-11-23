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
#include "common/time.h"
#include "ui/res/resmanager.h"
#include "ui/window/windowmanager.h"

#include <chrono>
#include <string>

namespace Ui {

// The status bar's text: the real status line, and over it a temporary message - the
// feedback an action gives on what it just did - that expires back to it
class StatusLine final : private Common::NonCopyable {
public:
    StatusLine(Ui::Res::ResManager & resManager, Ui::Window::WindowManager & windowManager)
        : m_resManager(resManager)
        , m_windowManager(windowManager)
    {
    }

    // True while a temporary message is showing. The host model-status updater checks
    // this so it does not clobber a temp message.
    [[nodiscard]] bool hasTemp() const { return !m_tempText.empty(); }

    // The real status line. While a temp message shows it waits underneath and is
    // what the expiry restores, so an update made during one is not lost
    void set(const std::string & text)
    {
        if (m_tempText.empty()) {
            m_resManager.setStatusText(text);
            m_windowManager.requestContentRefresh();
        } else {
            m_lastText = text;
        }
    }

    // A temporary message, resolved text - the caller owns the locale lookup - over the
    // real status, which expire() puts back
    void showTemp(const std::string & text)
    {
        // Only a real status is worth restoring: while a temp one shows, the bar
        // holds a temp message, and saving that would restore it for good
        if (m_tempText.empty()) {
            m_lastText = m_resManager.statusText();
        }
        m_tempText   = text;
        m_tempExpiry = Common::Time::now() + TEMP_DURATION;
        m_resManager.setStatusText(text);
        m_windowManager.requestContentRefresh();
    }

    // A temporary message whose time has run out gives the real status back
    void expire()
    {
        if (!m_tempText.empty() && Common::Time::now() >= m_tempExpiry) {
            m_tempText.clear();
            m_resManager.setStatusText(m_lastText);
            m_windowManager.requestContentRefresh();
        }
    }

private:
    static constexpr std::chrono::seconds TEMP_DURATION { 1 };

    Ui::Res::ResManager &       m_resManager;
    Ui::Window::WindowManager & m_windowManager;
    Common::Time::point_t       m_tempExpiry;
    std::string                 m_tempText;
    std::string                 m_lastText; // real text to restore after a temp expires
};

} // namespace Ui
