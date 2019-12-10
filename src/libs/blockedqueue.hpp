#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

namespace ctguard::libs {

template<typename T>
class blocked_queue
{
    using queue_type = typename std::queue<T>;
    using size_type = typename queue_type::size_type;

  public:
    explicit blocked_queue() = default;

    blocked_queue(const blocked_queue & other) = delete;
    blocked_queue & operator=(const blocked_queue & other) = delete;
    blocked_queue(blocked_queue && other) = delete;
    blocked_queue & operator=(blocked_queue && other) = delete;

    [[nodiscard]] size_type size() const;
    [[nodiscard]] bool empty() const;

    void push(T && value);

    template<class... Args>
    void emplace(Args &&... args);

    // T & front();
    [[nodiscard]] const T & front() const;

    void pop();

    [[nodiscard]] T take();

    template<class Rep, class Period>
    [[nodiscard]] std::optional<T> take(const std::chrono::duration<Rep, Period> & wait_duration);

  private:
    queue_type m_queue;
    mutable std::mutex m_mutex;
    mutable std::condition_variable m_cv;
};

template<typename T>
typename blocked_queue<T>::size_type blocked_queue<T>::size() const
{
    std::lock_guard mg{ m_mutex };
    return m_queue.size();
}

template<typename T>
bool blocked_queue<T>::empty() const
{
    std::lock_guard mg{ m_mutex };
    return m_queue.empty();
}

template<typename T>
void blocked_queue<T>::push(T && value)
{
    std::lock_guard mg{ m_mutex };
    m_queue.push(std::forward<T>(value));
    m_cv.notify_all();
}

template<typename T>
template<class... Args>
void blocked_queue<T>::emplace(Args &&... args)
{
    std::lock_guard mg{ m_mutex };
    m_queue.emplace(std::forward<Args>(args)...);
    m_cv.notify_all();
}

// template <typename T>
// T & blocked_queue<T>::front()
//{
//    std::unique_lock mg { m_mutex };
//    while(m_queue.empty()) {
//        m_cv.wait(mg);
//    }
//    return m_queue.front();
//}

template<typename T>
const T & blocked_queue<T>::front() const
{
    std::unique_lock mg{ m_mutex };
    while (m_queue.empty()) {
        m_cv.wait(mg);
    }
    return m_queue.front();
}

template<typename T>
void blocked_queue<T>::pop()
{
    std::lock_guard mg{ m_mutex };
    if (!m_queue.empty()) {
        m_queue.pop();
    }
}

template<typename T>
T blocked_queue<T>::take()
{
    std::unique_lock mg{ m_mutex };

    while (m_queue.empty()) {
        m_cv.wait(mg);
    }

    T tmp{ std::move(m_queue.front()) };
    m_queue.pop();

    return tmp;
}

template<typename T>
template<class Rep, class Period>
std::optional<T> blocked_queue<T>::take(const std::chrono::duration<Rep, Period> & wait_duration)
{
    std::unique_lock mg{ m_mutex };

    if (m_queue.empty()) {
        m_cv.wait_for(mg, wait_duration);
    }

    if (m_queue.empty())
        return std::nullopt;

    T tmp{ std::move(m_queue.front()) };
    m_queue.pop();

    return tmp;
}

} /* namespace ctguard::libs */
