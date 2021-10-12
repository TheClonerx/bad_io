
#include <cstdio>
#include <iostream>

#include <system_error>

#include <tcx/async/ioring/close.hpp>
#include <tcx/async/ioring/open.hpp>
#include <tcx/async/ioring/read.hpp>
#include <tcx/is_service.hpp>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;
    static_assert(tcx::is_service_v<tcx::ioring_service, tcx::unsynchronized_execution_context>);

    char tmp_buf[1024] {};

    tcx::async_open(ctx, io_service, "/etc/os-release", "rb", [&ctx, &io_service, &tmp_buf](std::error_code ec, int fd) mutable {
        if (ec)
            throw std::system_error(ec);

        tcx::async_read(ctx, io_service, fd, +tmp_buf, sizeof(tmp_buf), [&ctx, &io_service, fd, &tmp_buf](std::error_code ec, std::size_t bytes_read) {
            if (ec)
                throw std::system_error(ec);
            std::printf("%.*s\n", static_cast<int>(bytes_read), +tmp_buf);

            tcx::async_close(ctx, io_service, fd, [](std::error_code ec) { 
                if (ec)
            throw std::system_error(ec); });
        });
    });

    do {
        io_service.poll(ctx);
    } while (ctx.run());
}