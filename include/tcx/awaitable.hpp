#ifndef TCX_AWAITABLE_HPP
#define TCX_AWAITABLE_HPP

#include <coroutine>
#include <exception>
#include <future>
#include <type_traits>
#include <utility>
#include <variant>

#include <tcx/unique_function.hpp>

namespace tcx {

template <typename T>
class awaitable_promise;

template <typename Result>
class awaitable;

template <typename T>
class awaitable_promise_base {
public:
    using result_type = std::remove_cv_t<T>;

protected:
    using stored_type = std::conditional_t<std::is_void_v<result_type>, std::monostate, std::conditional_t<std::is_reference_v<result_type>, result_type *, result_type>>;

    /*
        first state (0): no_state, promise is not fullfilled
        second state (1): promise_already_satisfied, the coroutine exited without an exception
        third state (2): promise_already_satisfied, the coroutine exited with an exception
        fourth state (3): future_already_retrieved, the future (awaitable<T>) retrieved the value of state 1 or 2
    */
    using state_type = std::variant<std::monostate, stored_type, std::exception_ptr, std::monostate>;

private:
    auto coroutine_handle() const noexcept
    {
        return std::coroutine_handle<awaitable_promise<result_type>>::from_promise(static_cast<awaitable_promise<result_type> &>(*this));
    }

public:
    [[nodiscard]] auto get_return_object() const noexcept
    {
        return awaitable<result_type> { coroutine_handle() };
    }

    [[nodiscard]] bool is_ready() const noexcept
    {
        auto state_index = m_state.index();
        return state_index == 1 || state_index == 2;
    }

    [[nodiscard]] result_type get()
    {
        auto state_index = m_state.index();
        if (state_index == 0)
            throw std::future_error { std::future_errc::no_state };
        else if (state_index == 1) {
            if constexpr (std::is_void_v<result_type>) {
                m_state.template emplace<3>();
                coroutine_handle().destroy();
                return;
            } else if constexpr (std::is_reference_v<result_type>) {
                result_type result = *std::get<1>(m_state);
                m_state.template emplace<3>();
                coroutine_handle().destroy();
                return result;
            } else {
                result_type result = std::move(std::get<1>(m_state));
                m_state.template emplace<3>();
                coroutine_handle().destroy();
                return result;
            }
        } else if (state_index == 2) {
            std::exception_ptr e = std::get<2>(m_state);
            m_state.template emplace<3>();
            coroutine_handle().destroy();
            std::rethrow_exception(e);
        } else {
            throw std::future_error { std::future_errc::future_already_retrieved };
        }
    }

    [[nodiscard]] auto initial_suspend() const noexcept
    {
        return std::suspend_always {};
    }

    void unhandled_exception() noexcept
    {
        m_state.template emplace<2>(std::current_exception());
    }

    [[nodiscard]] auto final_suspend() const noexcept
    {
        return std::suspend_always {};
    }

protected:
    state_type m_state;
};

template <>
class awaitable_promise<void> final : public awaitable_promise_base<void> {
public:
    using typename awaitable_promise_base<void>::result_type;

private:
    using typename awaitable_promise_base<void>::state_type;

public:
    void return_void() noexcept
    {
        m_state.template emplace<1>();
    }

private:
    using awaitable_promise_base<void>::m_state;
};

template <typename T>
class awaitable_promise final : public awaitable_promise_base<T> {
public:
    using typename awaitable_promise_base<T>::result_type;

private:
    using typename awaitable_promise_base<void>::state_type;

public:
    void return_value(result_type value)
    {
        m_state.template emplace<1>(value);
    }

private:
    using awaitable_promise_base<T>::m_state;
};

template <typename Result>
class awaitable {
public:
    using result_type = std::remove_cv_t<Result>;
    using promise_type = awaitable_promise<result_type>;

    explicit awaitable(std::coroutine_handle<promise_type> handle) noexcept
        : m_handle { handle }
    {
    }

public:
    constexpr awaitable() noexcept = default;

    awaitable(awaitable<Result> const &) = delete;

    constexpr awaitable(awaitable<Result> &&other) noexcept
        : m_handle { std::exchange(other.m_handle, nullptr) }
    {
    }

    awaitable<Result> &operator=(awaitable<Result> const &) = delete;

    constexpr awaitable<Result> &operator=(awaitable<Result> &&other) noexcept
    {
        m_handle = std::exchange(other.m_handle, nullptr);
        return *this;
    }

    ~awaitable()
    {
        if (m_handle) {
            m_handle.destroy();
            m_handle = nullptr;
        }
    }

    [[nodiscard]] bool is_ready() const
    {
        if (m_handle) {
            return m_handle.promise().is_read();
        } else {
            throw std::future_error { std::future_errc::broken_promise };
        }
    }

    [[nodiscard]] result_type get()
    {
        if (m_handle) {
            return m_handle.promise().get();
        } else {
            throw std::future_error { std::future_errc::broken_promise };
        }
    }

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return m_handle;
    }

    template <typename E>
    void post_into(E &executor) &&noexcept
    {
        executor.post(std::exchange(m_handle, nullptr));
    }

private:
    std::coroutine_handle<promise_type> m_handle = nullptr;
};

} // namespace tcx

#endif