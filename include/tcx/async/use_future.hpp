#ifndef TCX_ASYNC_USE_FUTURE_HPP
#define TCX_ASYNC_USE_FUTURE_HPP

#include <exception>
#include <future>
#include <system_error>

namespace tcx {

template <typename T>
struct using_future_t {

    explicit using_future_t() = default;

    std::future<T> async_result()
    {
        return m_promise.get_future();
    }

    void operator()(std::error_code ec, T result)
    {
        if (ec) {
            try {
                throw std::system_error(ec);
            } catch (...) {
                m_promise.set_exception(std::current_exception());
            }
        } else {
            m_promise.set_value(std::move(result));
        }
    }

private:
    std::promise<T> m_promise;
};

struct use_future_t {
    explicit constexpr use_future_t() noexcept = default;

    template <typename T>
    using_future_t<T> async_transform() const
    {
        return using_future_t<T> {};
    }
};

inline constexpr auto use_future = use_future_t {};

}

#endif