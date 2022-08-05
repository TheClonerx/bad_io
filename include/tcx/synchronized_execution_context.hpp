#ifndef TCX_SYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_SYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <oneapi/tbb/concurrent_queue.h>

#include <tcx/unique_function.hpp>
#include <type_traits>

namespace tcx {

class synchronized_execution_context {
public:
    using function_storage = tcx::unique_function<void()>;
    static_assert(std::is_default_constructible_v<function_storage>, "function_storage type must be default constructible");

    synchronized_execution_context() = default;

    synchronized_execution_context(synchronized_execution_context const &) = delete;
    synchronized_execution_context(synchronized_execution_context &&) = delete;

    synchronized_execution_context &operator=(synchronized_execution_context const &) = delete;
    synchronized_execution_context &operator=(synchronized_execution_context &&other) noexcept = delete;

    template <typename F>
    void post(F &&f) requires(std::is_invocable_r_v<void, F>)
    {
        m_function_queue.emplace(std::forward<F>(f));
    }

    [[nodiscard]] std::size_t run();

    ~synchronized_execution_context() = default;

private:
    oneapi::tbb::concurrent_queue<function_storage> m_function_queue;
};

} // namespace tcx
#endif
