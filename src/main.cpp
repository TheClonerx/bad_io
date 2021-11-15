
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <liburing.h>
#include <memory>
#include <system_error>

#include <tcx/async/detached.hpp>
#include <tcx/async/ioring/close.hpp>
#include <tcx/async/ioring/open.hpp>
#include <tcx/async/ioring/read.hpp>
#include <tcx/async/ioring/stat.hpp>
#include <tcx/is_service.hpp>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

#include <fcntl.h>
#include <sys/stat.h>

void read_everything(tcx::unsynchronized_execution_context &ctx, tcx::ioring_service &io_service, int fd, std::uint64_t size)
{
    constexpr std::uint64_t chunk_size = 1024 * 8;

    std::printf("Size is %" PRIu64 " bytes, this should take %" PRIu64 " reads\n", size, size / chunk_size);

    auto buff = new char[size];
    auto counter = new std::atomic_size_t(size / chunk_size);

    for (std::uint64_t i = 0; i < size; i += chunk_size) {
        tcx::async_read(ctx, io_service, fd, buff + i, chunk_size, i, [&ctx, &service = io_service, buff, counter, fd](std::error_code ec, std::int64_t bytes_read) {
            if (ec)
                throw std::system_error(ec);
            std::printf("Read %" PRIu64 " bytes\n", bytes_read);

            if (counter->fetch_sub(1) == 1) {
                std::printf("Done!\n");
                delete[] buff;
                delete counter;

                tcx::async_close(ctx, service, fd, tcx::detached);
            }
        });
    }
}

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;
    static_assert(tcx::is_service_v<tcx::ioring_service>);

    tcx::async_open(ctx, io_service, "/home/joseh/Downloads/Fedora-Workstation-Live-x86_64-35_Beta-1.2.iso", "rb", [&ctx, &io_service](std::error_code ec, int fd) mutable {
        if (ec)
            throw std::system_error(ec);

        auto statbuf = std::make_unique<struct ::stat>();
        auto p = statbuf.get();
        tcx::async_statat(ctx, io_service, fd, "", p, AT_EMPTY_PATH, [&ctx, &io_service, fd, statbuf = std::move(statbuf)](std::error_code ec) {
            if (ec)
                throw std::system_error(ec);

            read_everything(ctx, io_service, fd, statbuf->st_size);
        });
    });

    for (;;) {
        bool const io_pending = io_service.pending();
        bool const tasks_pending = ctx.pending();

        if (!(io_pending || tasks_pending))
            break;

        if (io_pending)
            try {
                io_service.poll();
            } catch (std::exception const &e) {
                std::fprintf(stderr, "error: %s\n", e.what());
            }
        if (ctx.pending())
            try {
                ctx.run();
            } catch (std::exception const &e) {
                std::fprintf(stderr, "error: %s\n", e.what());
            }
    }
}
