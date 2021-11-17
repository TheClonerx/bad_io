#ifndef TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <queue>

#include <tcx/unique_function.hpp>

namespace tcx {

class unsynchronized_execution_context {
public:
    using function_storage = tcx::unique_function<void()>;

    unsynchronized_execution_context() = default;

    unsynchronized_execution_context(unsynchronized_execution_context const &) = delete;
    unsynchronized_execution_context(unsynchronized_execution_context &&) noexcept = default;

    unsynchronized_execution_context &operator=(unsynchronized_execution_context const &) = delete;
    unsynchronized_execution_context &operator=(unsynchronized_execution_context &&other) noexcept = default;

    template <typename F>
    void post(F &&f) requires(std::is_invocable_r_v<void, F>)
    {
        m_function_queue.emplace(std::forward<F>(f));
    }

    std::size_t run();
    std::size_t pending() const
    {
        return m_function_queue.size();
    }

    ~unsynchronized_execution_context() = default;

private:
    std::queue<function_storage> m_function_queue;
};

}

#endif
