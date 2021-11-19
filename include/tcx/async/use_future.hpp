#ifndef TCX_ASYNC_USE_FUTURE_HPP
#define TCX_ASYNC_USE_FUTURE_HPP

#include <exception>
#include <future>
#include <system_error>

namespace tcx {

/**
 * @brief Wraps an asynchronous operation to use `std::future<R>`.

 * @tparam R Result type.
 */
template <typename R>
struct using_future_t {

    explicit using_future_t() = default;

    /**
     * @brief Returns the future back to the caller.
     */
    std::future<R> async_result()
    {
        return m_promise.get_future();
    }

    /**
     * @brief Sets the result of the operation to the promise

     * In case of an error, an `std::system_error` exception is set with the value of `error`.
     * Otherwise `value` is moved into the promise.
     * @param error
     * @param value
     */
    void operator()(std::error_code error, R value)
    {
        if (error) {
            try {
                throw std::system_error(error);
            } catch (...) {
                m_promise.set_exception(std::current_exception());
            }
        } else {
            m_promise.set_value(std::move(value));
        }
    }

private:
    std::promise<R> m_promise;
};

/**
 * @brief Transforms an asynchronous operation to use `std::future<R>`.

 * The use_future_t class is used to indicate that an asynchronous operation
 * should return a `std::future<R>` object. A `tcx::use_future_t` object may be passed as
 * a completion handler to an asynchronous operation, typically using the
 * special value `tcx::use_future`.

 * @see tcx::using_future_t
 * @see tcx::use_future
 */
struct use_future_t {
    explicit constexpr use_future_t() noexcept = default;

    template <typename R>
    using_future_t<R> async_transform() const
    {
        return using_future_t<R> {};
    }
};

/**
 * @brief Specifies that an asynchronous operation uses an `std::future` object.
 * @ingroup completion_objects
 * @see tcx::use_future_t
 */
inline constexpr auto use_future = use_future_t {};

}

#endif