#pragma once

#include "nexus/exec/policy.hpp"
#include "nexus/exec/task.hpp"
#include "nexus/private/exec/queue.hpp"

#include <any>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <utility>

namespace nexus::exec {

/**
 * @brief Thread safe task queue.
 *
 * @tparam R Task result type.
 * @tparam P Queue policy.
 */
template <typename R = std::any, TaskPolicy P = TaskPolicy::FIFO>
class TaskQueue {
  public:
    using Inner = detail::TaskQueueInner<R, P>;

  private:
    Inner _inner;
    std::mutex _lock;
    std::condition_variable _cond;

  public:
    TaskQueue() = default;

    /**
     * @brief Add a task to the queue.
     *
     * @tparam Args Task construct arguments type.
     * @param args Task construct arguments.
     */
    template <typename... Args> auto emplace(Args &&...args) -> void {
        push(Task<R>(std::forward<Args>(args)...));
    }

    /**
     * @brief Add a task to the queue.
     *
     * @param task Task object.
     */
    auto push(Task<R> &&task) -> void {
        auto guard = std::unique_lock(_lock);
        _inner.push(std::move(task));
        guard.unlock();

        _cond.notify_one();
    }

    /**
     * @brief  Pop one task (wait until queue is ready or timeout).
     *
     * @param timeout Wait timeout.
     * @return std::optional<Task<R>> Task object.
     */
    template <typename Rep, typename Period>
    auto pop_for(const std::chrono::duration<Rep, Period> &timeout)
        -> std::optional<Task<R>> {
        auto guard = std::unique_lock(_lock);
        auto status = _cond.wait_for(
            guard, timeout, [this]() { return this->_inner.size() > 0; });

        if (!status) {
            return {};
        }

        return _inner.pop();
    }

    /**
     * @brief Pop one task (wait until queue is ready).
     *
     * @return Task<R> Task object.
     */
    auto pop() -> Task<R> {
        auto guard = std::unique_lock(_lock);
        _cond.wait(guard, [this]() { return this->_inner.size() > 0; });

        return _inner.pop();
    }

    /**
     * @brief Get task count.
     *
     * @return std::size_t Task count.
     */
    auto size() {
        auto guard = std::lock_guard(_lock);
        return _inner.size();
    }
};

} // namespace nexus::exec
