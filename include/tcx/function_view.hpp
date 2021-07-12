#pragma once

#include <functional>
#include <type_traits>
// #include <utility>

namespace tcx {

template <typename T>
struct function_reference;

template <typename R, typename... Args>
struct function_reference<R(Args...)> {
public:
    using result_type = R;

private:
    using function_pointer_type = R (*)(void *, Args &&...);

public:
    constexpr function_reference() noexcept = default;
    constexpr function_reference(std::nullptr_t) noexcept
        : function_reference()
    {
    }

    constexpr function_reference(function_reference const &) noexcept = default;
    constexpr function_reference(function_reference &&other) noexcept
        : m_data { std::exchange(other.m_data, nullptr) }
        , m_func { std::exchange(other.m_func, nullptr) }
    {
    }

    constexpr function_reference &operator=(function_reference const &) noexcept = default;
    constexpr function_reference &operator=(function_reference &&other) noexcept
    {
        swap(other);
        return *this;
    }

    template <typename F>
    function_reference(F &f) noexcept requires std::is_invocable_r_v<R, F, Args...>
    {
        if constexpr (std::is_convertible_v<F, result_type (*)(Args...)> && !std::is_constant_evaluated()) {
            m_data = static_cast<void *>(static_cast<R (*)(Args...)>(f));
            m_func = +[](void const *ptr, Args &&...args) -> R {
                auto *f = reinterpret_cast<result_type (*)(Args...)>(ptr);
                return (*f)(std::forward<Args>(args)...);
            };
        } else {
            m_data = static_cast<void *>(&f);
            m_func = +[](void *data, Args &&...args) -> R {
                auto const &f = *reinterpret_cast<F *>(data);
                return f(std::forward<Args>(args)...);
            };
        }
    }

    constexpr void swap(function_reference &other) noexcept
    {
        using std::swap;
        swap(m_data, other.m_data);
        swap(m_func, other.m_func);
    }

    result_type operator()(Args &&...args)
    {
        if (!m_func)
            throw std::bad_function_call {};
        return m_func(m_data, std::forward<Args>(args)...);
    }

    constexpr explicit operator bool() const noexcept
    {
        return m_func;
    }

private:
    void *m_data = nullptr;
    function_pointer_type m_func = nullptr;
};

template <typename R, typename... Args>
struct function_reference<R(Args...) const> {
public:
    using result_type = R;

private:
    using function_pointer_type = R (*)(void const *, Args &&...);

public:
    constexpr function_reference() noexcept = default;
    constexpr function_reference(std::nullptr_t) noexcept
        : function_reference()
    {
    }

    constexpr function_reference(function_reference const &) noexcept = default;
    constexpr function_reference(function_reference &&other) noexcept
        : m_data { std::exchange(other.m_data, nullptr) }
        , m_func { std::exchange(other.m_func, nullptr) }
    {
    }

    constexpr function_reference &operator=(function_reference const &) noexcept = default;
    constexpr function_reference &operator=(function_reference &&other) noexcept
    {
        swap(other);
        return *this;
    }

    template <typename F>
    function_reference(F &&f) requires(std::is_invocable_r_v<R, F, Args...> &&std::is_rvalue_reference_v<F>) = delete;

    template <typename F>
    constexpr function_reference(F const &f) noexcept requires std::is_invocable_r_v<R, F, Args...>
    {
        if constexpr (std::is_convertible_v<F, result_type (*)(Args...)> && !std::is_constant_evaluated()) {
            m_data = static_cast<void const *>(static_cast<R (*)(Args...)>(f));
            m_func = +[](void const *ptr, Args &&...args) -> R {
                auto *f = reinterpret_cast<result_type (*)(Args...)>(ptr);
                return (*f)(std::forward<Args>(args)...);
            };
        } else {
            m_data = static_cast<void const *>(&f);
            m_func = +[](void const *data, Args &&...args) -> R {
                auto const &f = *reinterpret_cast<F const *>(data);
                return f(std::forward<Args>(args)...);
            };
        }
    }

    constexpr void swap(function_reference &other) noexcept
    {
        using std::swap;
        swap(m_data, other.m_data);
        swap(m_func, other.m_func);
    }

    result_type operator()(Args &&...args) const
    {
        if (!m_func)
            throw std::bad_function_call {};
        return m_func(m_data, std::forward<Args>(args)...);
    }

    constexpr explicit operator bool() const noexcept
    {
        return m_func;
    }

private:
    void const *m_data = nullptr;
    function_pointer_type m_func = nullptr;
};

} // namespace tcx
