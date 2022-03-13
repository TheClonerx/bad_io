
#include <tcx/async/detached.hpp>
#include <tcx/async/ioring.hpp>
#include <tcx/is_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

#include <fcntl.h>
#include <sys/stat.h>

void read_everything(tcx::unsynchronized_execution_context &ctx, tcx::ioring_service &io_service, int fd, std::uint64_t size, std::uint64_t chunk_size)
{
    std::printf("Size is %" PRIu64 " bytes, this should take %" PRIu64 " reads\n", size, size / chunk_size);

    auto buff = new char[size];
    auto counter = new std::atomic_size_t(size / chunk_size);

    for (std::uint64_t i = 0; i < size; i += chunk_size) {
        tcx::async_read(ctx, io_service, fd, buff + i, chunk_size, i, [&ctx, &io_service = io_service, buff, counter, fd, chunk_size, size](std::error_code ec, std::int64_t bytes_read) {
            if (ec)
                throw std::system_error(ec);

            auto count = counter->fetch_sub(1, std::memory_order_relaxed);
            std::printf("\r%6.2lf%%", static_cast<double>(size - count * chunk_size) / size * 100.0);

            if (count == 1) {
                std::printf(" Done!\n");
                delete[] buff;
                delete counter;

                tcx::async_close(ctx, io_service, fd, tcx::detached);
            }
        });
    }
}

template <typename... Runners>
void run_until_complete(Runners &...runners)
{
    // this looks cryptict
    for (;;) {
        bool keep_running = false;
        ([&runner = runners, &keep_running]() {
            if (runner.pending()) {
                try {
                    if constexpr (requires { runner.poll(); })
                        runner.poll();
                    else
                        runner.run();
                } catch (std::exception &e) {
                    std::fprintf(stderr, "uncaught exception: %s\n", e.what());
                }
                keep_running = true;
            }
        }(),
            ...);
        if (!keep_running)
            break;
    }
}

int main()
{
    tcx::unsynchronized_execution_context ctx;
    tcx::ioring_service io_service;
    static_assert(tcx::is_service_v<tcx::ioring_service>);

    tcx::async_open(ctx, io_service, "/home/joseh/Downloads/Fedora-LXQt-Live-x86_64-35-1.2.iso", "rb", [&ctx, &io_service](std::error_code ec, int fd) mutable {
        if (ec)
            throw std::system_error(ec);

        auto statbuf = std::make_unique<struct ::stat>();
        auto p = statbuf.get();
        tcx::async_statat(ctx, io_service, fd, "", p, AT_EMPTY_PATH, [&ctx, &io_service, fd, statbuf = std::move(statbuf)](std::error_code ec) {
            if (ec)
                throw std::system_error(ec);

            read_everything(ctx, io_service, fd, statbuf->st_size, statbuf->st_blksize);
        });
    });

    run_until_complete(ctx, io_service);
}
