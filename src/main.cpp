
#include <cstdio>
#include <iostream>

#include <system_error>
#include <unistd.h> // ::close

#include <tcx/async/ioring/open.hpp>
#include <tcx/is_service.hpp>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;
    static_assert(tcx::is_service_v<tcx::ioring_service, tcx::unsynchronized_execution_context>);

    tcx::async_open(ctx, io_service, "/etc/os-release", "rb", [](std::error_code ec, int fd) mutable {
        if (ec)
            throw std::system_error(ec);
        char tmp_buf[1024] {};

        auto sz = read(fd, tmp_buf, sizeof(tmp_buf));
        if (sz < 0)
            throw std::system_error(errno, std::system_category());
        std::printf("%.*s\n", static_cast<int>(sz), +tmp_buf);
        close(fd);
    });

    do {
        io_service.poll(ctx);
    } while (ctx.run());
}