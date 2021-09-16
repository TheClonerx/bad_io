
#include <iostream>
#include <tcx/unsynchronized_execution_context.hpp>

#include <tcx/services/ioring_service.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;

    ctx.run();
}
