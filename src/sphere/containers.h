/**
* @file containers.h
* @brief Sphere custom containers.
*/

#ifndef _INC_CONTAINERS_H
#define _INC_CONTAINERS_H

#include <deque>
#include <mutex>
#include <optional>

template<class T>
class ThreadSafeQueue
{
private:
    std::deque<T>        m_deque;
    mutable std::mutex   m_mutex;

public:
    ThreadSafeQueue() noexcept = default;
    ~ThreadSafeQueue() noexcept = default;

    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

public:
    // Append an element to the end of the queue
    void push(const T& value)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_deque.push_back(value);
    }

    void push(T&& value)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_deque.push_back(std::move(value));
    }

    // Erase elements or clear remaining (no-op in deque-backed model since elements are popped eagerly)
    void clean() noexcept
    {
        // Kept for backward compatibility with existing cleanup call sites
    }

    void clear() noexcept
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_deque.clear();
    }

    // Retrieve the number of elements in the queue
    [[nodiscard]] size_t size() const noexcept
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_deque.size();
    }

    // Determine if the queue is empty
    [[nodiscard]] bool empty() const noexcept
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        return m_deque.empty();
    }

    // Remove the first element from the queue
    void pop()
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deque.empty())
        {
            throw CSError(LOGL_ERROR, 0, "No elements to read from queue.");
        }
        m_deque.pop_front();
    }

    // Retrieve the first element in the queue
    [[nodiscard]] T front() const
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deque.empty())
        {
            throw CSError(LOGL_ERROR, 0, "No elements to read from queue.");
        }
        return m_deque.front();
    }

    // Atomic try-pop helper to safely fetch and remove in a single lock acquisition
    bool try_pop(T& out)
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (m_deque.empty())
        {
            return false;
        }
        out = std::move(m_deque.front());
        m_deque.pop_front();
        return true;
    }
};

#endif // _INC_CONTAINERS_H
