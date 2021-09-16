#ifndef TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <deque>
#include <memory>
#include <memory_resource>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include <tcx/is_service.hpp>
#include <tcx/unique_function.hpp>
#include <tcx/unique_types.hpp>

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
    void post(F &&f) requires(std::is_invocable_r_v<void, F> &&std::is_move_constructible_v<F>)
    {
        m_function_queue.emplace(std::forward<F>(f));
    }

    template <typename F>
    void post(F &&f) requires(std::is_invocable_v<F> && !std::is_invocable_r_v<void, F> && std::is_move_constructible_v<F>)
    {
        m_function_queue.emplace([f = std::forward<F>(f)]() mutable {
            f();
        });
    }

    void run();

    ~unsynchronized_execution_context() = default;

private:
    std::queue<function_storage> m_function_queue;
};
}
#endif
