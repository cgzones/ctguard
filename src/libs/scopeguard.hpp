#pragma once

#include <utility>

namespace ctguard::libs {

template<typename T, typename = void>
struct is_callable : std::is_function<T>
{};

template<typename T>
struct is_callable<T, typename std::enable_if<std::is_same<decltype(void(&T::operator())), void>::value>::type> : std::true_type
{};

template<typename T>
class scope_guard
{
  public:
    explicit scope_guard(T action) noexcept : m_action(std::move(action)), m_active{ true }
    {
        static_assert(is_callable<T>::value, "argument is not callable");
    }
    ~scope_guard() noexcept { fire(); }

    scope_guard(const scope_guard & other) = delete;
    scope_guard & operator=(const scope_guard & other) = delete;
    scope_guard(scope_guard && other) noexcept : m_action(std::move(other.m_action)), m_active{ other.m_active } { other.release(); }
    scope_guard & operator=(scope_guard && other) = delete;

    void release() noexcept { m_active = false; }
    void fire() noexcept
    {
        if (m_active) {
            m_action();
        }
        release();
    }

  private:
    T m_action;
    bool m_active;
};

template<typename T>
[[nodiscard, deprecated]] scope_guard<std::decay_t<T>> make_scope_guard(T && action) noexcept
{
    static_assert(is_callable<T>::value, "argument is not callable");
    return scope_guard<std::decay_t<T>>{ std::forward<T>(action) };
}

} /* namespace ctguard::libs */
