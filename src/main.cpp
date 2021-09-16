
#include <iostream>

#include <tcx/is_service.hpp>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;
    static_assert(tcx::is_service_v<tcx::ioring_service, tcx::unsynchronized_execution_context>);

    ctx.run();
}
