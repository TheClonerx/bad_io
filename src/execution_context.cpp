#include <tcx/synchronized_execution_context.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

std::size_t tcx::unsynchronized_execution_context::run()
{
    std::size_t count = 0;
    while (!m_function_queue.empty()) {
        auto f = std::move(m_function_queue.front());
        m_function_queue.pop();
        f();
        ++count;
    }
    return count;
}

std::size_t tcx::synchronized_execution_context::run()
{
    std::size_t count = 0;
    function_storage f;
    for (;;) {
        if (!m_function_queue.try_pop(f))
            break;
        f();
        ++count;
    }
    return count;
}