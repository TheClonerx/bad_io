
#include <array>
#include <tcx/services/ioring_service.hpp>
#include <tcx/unsynchronized_execution_context.hpp>

int main()
{
    std::array<char, 1024 * 8> buf;
    tcx::unsynchronized_execution_context ctx;
    auto& ioring = ctx.use_service<tcx::ioring_service>();
    ioring.async_open(0, "CMakeLists.txt", O_RDONLY, 0, [&ioring, &buf](int fd) {
        if (fd < 0)
            throw std::system_error(-fd, std::system_category(), "opening");
        ioring.async_read(0, fd, buf.data(), buf.size(), [&ioring, &buf](int bytes_read) {
            if (bytes_read < 0)
                throw std::system_error(-bytes_read, std::system_category(), "reading");
            ioring.async_write(0, STDOUT_FILENO, buf.data(), bytes_read, [](int bytes_written) {
                if (bytes_written < 0)
                    throw std::system_error(-bytes_written, std::system_category(), "writting");
            });
        });
    });
    ctx.run();
}
