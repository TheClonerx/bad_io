
#include <cstdio>

#include <tcx/async/detached.hpp>
#include <tcx/async/ioring.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

#include <fcntl.h>
#include <sys/stat.h>

static void read_everything(tcx::unsynchronized_execution_context &ctx, tcx::unsynchronized_uring_context<> &io_service, int fd, std::uint64_t size, std::uint64_t chunk_size)
{
    std::printf("Size is %" PRIu64 " bytes, this should take %" PRIu64 " reads\n", size, size / chunk_size);

    auto counter = new std::size_t { size / chunk_size };
    auto buff = new char[*counter * chunk_size];

    for (std::uint64_t i = 0; i < *counter; ++i) {
        tcx::async_read(ctx, io_service, fd, buff + i * chunk_size, chunk_size, i, [&ctx, &io_service = io_service, buff, counter, fd, chunk_size, size](std::variant<std::error_code, std::size_t> result) {
            if (result.index() == 0)
                throw std::system_error(std::get<0>(result));

            auto const count = *counter -= 1;
            std::printf("\r%6.2lf%%", static_cast<double>(size - count * chunk_size) / static_cast<double>(size) * 100.0);
            std::fflush(stdout);

            if (count == 0) {
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
            try {
                if (runner.pending())
                    keep_running = true;
            } catch (std::exception &e) {
                std::fprintf(stderr, "uncaught exception: %s\n", e.what());
                keep_running = true;
            }
        }(),
            ...);
        if (!keep_running)
            break;
    }
}

#include <tcx/async/epoll.hpp>

int main()
{
    tcx::unsynchronized_execution_context ctx;
    auto io_service = tcx::unsynchronized_uring_context<>::create(1024).value();

    constexpr tcx::native::c_string filepath = "/home/joseh/Downloads/Win10_21H2_EnglishInternational_x64.iso";
    tcx::async_open(ctx, io_service, filepath, "rb", [&ctx, &io_service](std::variant<std::error_code, tcx::native::handle_type> result) mutable {
        if (result.index() == 0) {
            auto const &code = std::get<0>(result);
            std::fprintf(stderr, "Failed to open %s: %s\n", filepath, code.message().c_str());
            throw std::system_error(code);
        }

        auto fd = std::get<1>(result);

        auto statbuf = std::make_unique<struct ::stat>();
        auto p = statbuf.get();
        tcx::async_statat(ctx, io_service, fd, "", p, AT_EMPTY_PATH, [&ctx, &io_service, fd, statbuf = std::move(statbuf)](std::variant<std::error_code, std::monostate> result) {
            if (result.index() == 0)
                throw std::system_error(std::get<0>(result));

            read_everything(ctx, io_service, fd, statbuf->st_size, statbuf->st_blksize);
        });
    });

    run_until_complete(ctx, io_service);
}
