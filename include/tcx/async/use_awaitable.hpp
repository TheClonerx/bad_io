#ifndef TCX_ASYNC_USE_AWAITABLE_HPP
#define TCX_ASYNC_USE_AWAITABLE_HPP

#include <coroutine>
#include <memory>
#include <system_error>
#include <variant>

namespace tcx {

template <typename T>
struct using_awaitable_t {
private:
    struct state_type {
        std::variant<std::monostate, std::error_code, T> result;
        std::coroutine_handle<> m_coroutine = std::noop_coroutine();
    };

public:
    explicit using_awaitable_t()
        : m_state(std::make_shared<state_type>())
    {
    }

    auto async_result()
    {
        struct awaitable {

            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> coroutine) { m_state->m_coroutine = coroutine; }

            T await_resume() const
            {
                if (m_state->m_result.index() == 1)
                    throw std::system_error(std::get<1>(m_state->m_result));
                return std::get<2>(std::move(m_state->m_result));
            }

        private:
            std::shared_ptr<state_type> m_state;
        };

        return awaitable { m_state };
    }

    void operator()(std::error_code ec, T result)
    {
        if (ec)
            m_state->m_result.template emplace<1>(ec);
        else
            m_state->m_result.template emplace<2>(std::move(result));
        m_state->m_coroutine.resume();
    }

private:
    std::shared_ptr<state_type> m_state;
};

struct use_awaitable_t {
    explicit constexpr use_awaitable_t() noexcept = default;

    template <typename T>
    using_awaitable_t<T> async_transform()
    {
        return using_awaitable_t<T> {};
    }
};

inline constexpr auto use_awaitable = use_awaitable_t {};

}

#endif