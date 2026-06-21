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

#include "common/system.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace Common {

/**
 * @brief Background task pool with per-task CPU budgeting and safe shutdown.
 *
 * A fixed pool of max(1, cpuCores - 2) worker threads drains a FIFO queue of
 * tasks. Two cores are left for the UI/render thread and the OS. Used to keep
 * heavy work (resource loads, file I/O, teardown, session writes, etc.) off the
 * main UI thread without proliferating per-subsystem threads.
 *
 * CPU budgeting: each task declares how many cores it intends to use. A task is
 * admitted only when its request fits the currently-free budget, so the sum of
 * in-flight demands never exceeds the pool size and the CPU is not
 * oversubscribed. Requests are clamped to [1, budget] so a task can never block
 * forever by asking for more cores than exist. Admission is FIFO: a large task
 * at the head holds back cheaper tasks behind it until enough cores free up.
 *
 * Lifetime: a process-scoped singleton (see backgroundWorker()). The destructor
 * signals stop, drains remaining queued tasks, then joins - workers NEVER
 * outlive the process. Exit will block briefly if a slow task is still running,
 * which is the acceptable trade-off vs detach() leaving workers running after
 * process teardown has begun.
 *
 * Move-only state in tasks: std::function requires the callable to be
 * copy-constructible, so move-only captures (unique_ptrs, std::thread, etc.)
 * must be wrapped in std::shared_ptr by the caller before capture.
 */
class BackgroundWorker final {
public:
    BackgroundWorker()
        : m_budget((System::cpuCores() > 2) ? System::cpuCores() - 2 : 1)
        , m_available(m_budget)
    {
        for (unsigned int i = 0; i < m_budget; ++i) {
            m_pool.emplace_back(&BackgroundWorker::loop, this);
        }
    }

    ~BackgroundWorker()
    {
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_stop.store(true);
        }
        m_cv.notify_all();
        for (std::thread & worker : m_pool) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    BackgroundWorker(const BackgroundWorker &)             = delete;
    BackgroundWorker(BackgroundWorker &&)                  = delete;
    BackgroundWorker & operator=(const BackgroundWorker &) = delete;
    BackgroundWorker & operator=(BackgroundWorker &&)      = delete;

    /**
     * @brief Enqueue a task declaring how many cores it will use.
     *
     * Returns immediately; the task runs once enough of the pool's core budget
     * is free. Tasks posted before destruction are guaranteed to run; tasks
     * posted after stop are silently dropped (only happens during shutdown).
     */
    void post(unsigned int threads, std::function<void()> task)
    {
        const unsigned int cost = (threads == 0) ? 1 : ((threads > m_budget) ? m_budget : threads);
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stop.load()) {
                return;
            }
            m_queue.emplace(cost, std::move(task));
        }
        m_cv.notify_all();
    }

    // Single-core task (the common case): file IO, session writes, teardown.
    void post(std::function<void()> task) { post(1, std::move(task)); }

    // Total cores this pool will hand out across all in-flight tasks.
    [[nodiscard]] unsigned int budget() const { return m_budget; }

private:
    void loop()
    {
        while (true) {
            std::function<void()> task;
            unsigned int          cost = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                // Work when the head task fits the free budget; exit only once
                // stop is set AND the queue is fully drained. A head task that
                // does not yet fit waits for a sibling to release cores - which
                // it must, since an idle pool restores m_available to m_budget
                // and every queued cost is clamped to <= m_budget.
                m_cv.wait(lock, [this]() {
                    if (!m_queue.empty() && m_queue.front().first <= m_available) {
                        return true;
                    }
                    return m_stop.load() && m_queue.empty();
                });
                if (m_queue.empty()) {
                    return; // stop set and drained
                }
                cost = m_queue.front().first;
                task = std::move(m_queue.front().second);
                m_queue.pop();
                m_available -= cost;
            }
            task();
            {
                const std::lock_guard<std::mutex> lock(m_mutex);
                m_available += cost;
            }
            m_cv.notify_all();
        }
    }

    const unsigned int                                         m_budget;
    unsigned int                                               m_available;
    std::queue<std::pair<unsigned int, std::function<void()>>> m_queue;
    std::mutex                                                 m_mutex;
    std::condition_variable                                    m_cv;
    std::atomic<bool>                                          m_stop { false };
    std::vector<std::thread>                                   m_pool;
};

/**
 * @brief Process-scoped BackgroundWorker singleton accessor.
 *
 * Lazy-init on first call (C++11-safe), one instance per process. The static
 * destructs after main() returns, AFTER all stack and member objects are gone -
 * so anything that calls backgroundWorker().post() during teardown still gets a
 * live worker.
 */
inline BackgroundWorker & backgroundWorker()
{
    static BackgroundWorker instance;
    return instance;
}

} // namespace Common
