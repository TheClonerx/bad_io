#ifndef TCX_SYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_SYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <moodycamel/concurrentqueue.h>

#include <tcx/unique_function.hpp>

namespace tcx {

class synchronized_execution_context {
public:
    using function_storage = tcx::unique_function<void()>;

    synchronized_execution_context() = default;

    synchronized_execution_context(synchronized_execution_context const &) = delete;
    synchronized_execution_context(synchronized_execution_context &&) = delete;

    synchronized_execution_context &operator=(synchronized_execution_context const &) = delete;
    synchronized_execution_context &operator=(synchronized_execution_context &&other) noexcept = delete;

    template <typename F>
    void post(F &&f) requires(std::is_invocable_r_v<void, F> &&std::is_move_constructible_v<F>)
    {
        m_function_queue.enqueue(function_storage(std::forward<F>(f)));
    }

    std::size_t run();

    ~synchronized_execution_context() = default;

private:
    moodycamel::ConcurrentQueue<function_storage> m_function_queue;
};

}
#endif
