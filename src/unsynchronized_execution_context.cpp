#include <tcx/unsynchronized_execution_context.hpp>

void tcx::unsynchronized_execution_context::run()
{
    while (!m_function_queue.empty()) {
        auto f = std::move(m_function_queue.front());
        m_function_queue.pop();
        f();
    }
}