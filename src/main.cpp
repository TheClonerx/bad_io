#include <array>

#include <tcx/async/open.hpp>
#include <tcx/async/read.hpp>
#include <tcx/async/write.hpp>
#include <tcx/awaitable.hpp>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

template <typename E>
tcx::awaitable<void> run(E& executor)
{
    std::exception_ptr e;
    tcx::native_handle_type fd = tcx::invalid_handle;

    try {
        std::array<char, 1024 * 8> buf;
        fd = co_await tcx::async_open(executor, "CMakeLists.txt", tcx::open_mode::read, tcx::use_awaitable);
        std::size_t bytes_read = co_await tcx::async_read(executor, fd, buf.data(), buf.size(), tcx::use_awaitable);
        co_await tcx::async_write(executor, STDOUT_FILENO, buf.data(), bytes_read, tcx::use_awaitable);
    } catch (...) {
        e = std::current_exception();
    }
    if (fd != -1) {
        try {
        } catch (std::system_error&) {
        }
    }
    if (e)
        std::rethrow_exception(e);
    co_return;
}

int main()
{
    tcx::unsynchronized_execution_context ctx;
    run(ctx).post_into(ctx);
    ctx.run();
}
