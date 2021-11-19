#ifndef TCX_ASYNC_USE_AWAITABLE_HPP
#define TCX_ASYNC_USE_AWAITABLE_HPP

#include <coroutine>

#include <memory>
#include <system_error>
#include <variant>

#ifndef NDEBUG
#include <future>
#endif

namespace tcx {

/**
 * @brief Wraps an asynchronous operation to use an awaitable.

 * @tparam R Result type.
 * @see tcx::use_awaitable_t
 */
template <typename R>
struct using_awaitable_t {
private:
    struct state_type {
        std::variant<std::monostate, std::error_code, R> result;
        std::coroutine_handle<> m_coroutine = std::noop_coroutine();
    };

public:
    explicit using_awaitable_t()
        : m_state(std::make_shared<state_type>())
    {
    }

    /**
     * @brief Returns an awaitable back to the caller.

     * After the coroutine is resumed by awaiting this value,
     * if an error is stored, a `std::system_error` exception will be thrown using it's value.
     * Otherwise, the result value of the operation is returned.

     * Awaiting this value twice is undefined behavior.
     * @return implentation defined
     */
    auto async_result()
    {
        struct awaitable {

            constexpr bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<> coroutine) { m_state->m_coroutine = coroutine; }

            R await_resume()
            {
                if (m_state->m_result.index() == 1) {
                    auto ec = std::get<1>(m_state->m_result);
                    m_state->template emplace<0>();
                    throw std::system_error(ec);
                } else if (m_state->m_result.index() == 2) {
                    R result = std::get<2>(std::move(m_state->m_result));
                    m_state->template emplace<0>();
                    return result;
                }
#ifndef NDEBUG
                else
                    throw std::system_error(std::make_error_code(std::future_errc::future_already_retrieved));
#endif
            }

        private:
            std::shared_ptr<state_type> m_state;
        };

        return awaitable { m_state };
    }

    /**
     * @brief Sets the result of the operation to the shared state.

     * In case of an error, an error is set to the shared state with the value of `error`.
     * Otherwise `result` is moved into the shared state.
     * Finally, the coroutine is resumed.
     * @param error
     * @param result
     */
    void operator()(std::error_code error, R result)
    {
#ifndef NDEBUG
        if (m_state->m_result.index() != 0)
            throw std::system_error(std::make_error_code(std::future_errc::promise_already_satisfied));
#endif
        if (error)
            m_state->m_result.template emplace<1>(error);
        else
            m_state->m_result.template emplace<2>(std::move(result));
        m_state->m_coroutine.resume();
    }

private:
    std::shared_ptr<state_type> m_state;
};

/**
 * @brief Transforms an asynchronous operation to use an awaitable.
 * @see tcx::using_awaitable_t
 */
struct use_awaitable_t {
    explicit constexpr use_awaitable_t() noexcept = default;

    template <typename R>
    using_awaitable_t<R> async_transform()
    {
        return using_awaitable_t<R> {};
    }
};

/**
 * @brief Specifies that an asynchronous operation uses an awaitable.
 * @ingroup completion_objects
 * @see tcx::use_awaitable_t
 */
inline constexpr auto use_awaitable = use_awaitable_t {};

}

#endif