
#include <iostream>
#include <tcx/unsynchronized_execution_context.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;

    ctx.post([&ctx]() -> decltype(auto) {
        ctx.post([]() -> decltype(auto) { return std::cout << std::endl; });
        return std::cout << "Hello World!";
    });

    ctx.run();
}
