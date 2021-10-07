#ifndef TCX_SERVICES_IORING_SERVICE_HPP
#define TCX_SERVICES_IORING_SERVICE_HPP

#include <algorithm>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <fcntl.h> // only for AT_FDCWD
// #include <linux/time_types.h>
// #include <sys/types.h>

#include <liburing.h>

#include <tcx/unique_function.hpp>

namespace tcx {
template <typename F>
concept ioring_completion_handler = std::is_invocable_v<F, std::int32_t>;

class ioring_service {
public:
    using native_handle_type = int;
    static constexpr native_handle_type invalid_handle = -1;

    ioring_service()
        : ioring_service(1024)
    {
    }

    explicit ioring_service(std::uint32_t entries)
        : m_uring(setup_rings(entries))
    {
    }

    ioring_service(ioring_service const &other) = delete;
    ioring_service(ioring_service &&other) noexcept
        : m_uring {
            std::exchange(other.m_uring.sq, io_uring_sq {}),
            std::exchange(other.m_uring.cq, io_uring_cq {}),
            std::exchange(other.m_uring.flags, 0),
            std::exchange(other.m_uring.ring_fd, -1),
            std::exchange(other.m_uring.features, 0),
            {} /*padding*/
        }
        , m_last_id { std::exchange(other.m_last_id, 0) }
        , m_completions(std::move(other.m_completions))

    {
    }

    ioring_service &operator=(ioring_service const &other) = delete;
    ioring_service &operator=(ioring_service &&other) noexcept
    {
        if (this == &other)
            return *this;

        if (native_handle() != invalid_handle)
            io_uring_queue_exit(&m_uring);

        m_uring = io_uring {
            std::exchange(other.m_uring.sq, io_uring_sq {}),
            std::exchange(other.m_uring.cq, io_uring_cq {}),
            std::exchange(other.m_uring.flags, 0),
            std::exchange(other.m_uring.ring_fd, -1),
            std::exchange(other.m_uring.features, 0),
            {} /*padding*/
        };
        m_last_id = std::exchange(other.m_last_id, 0);
        m_completions = std::move(other.m_completions);
        return *this;
    }

public:
    native_handle_type native_handle() noexcept
    {
        return m_uring.ring_fd;
    }

    // does nothing (asynchronously)
    template <tcx::ioring_completion_handler F>
    auto async_noop(F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_nop(&op);

        return submit(op, std::forward<F>(f));
    }

    // preadv2(2)
    template <tcx::ioring_completion_handler F>
    auto async_readv(int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_readv(&op, fd, iov, len, offset);
        op.rw_flags = flags;

        return submit(op, std::forward<F>(f));
    }

    // pwritev2(2)
    template <tcx::ioring_completion_handler F>
    auto async_writev(int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_writev(&op, fd, iov, len, offset);
        op.rw_flags = flags;

        return submit(op, std::forward<F>(f));
    }

    // fsync(2)
    template <tcx::ioring_completion_handler F>
    auto async_fsync(int fd, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fsync(&op, fd, flags);

        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    auto async_poll_add(int fd, std::uint32_t events, F &&f)
    {

        io_uring_sqe op {};
        io_uring_prep_poll_add(&op, fd, events);

        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    auto async_poll_remove(std::uint64_t entry_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_poll_remove(&op, nullptr); // this takes a pointer (which might not be 64 bits) as addr (__u64)
        op.addr = entry_id; // set the entry_id directly

        return submit(op, std::forward<F>(f));
    }

    // epoll_ctl(2)
    template <tcx::ioring_completion_handler F>
    auto async_epoll_ctl(int epoll_fd, int op, int fd, epoll_event *event, F &&f)
    {
        io_uring_sqe operation {};
        io_uring_prep_epoll_ctl(&operation, epoll_fd, fd, op, event);

        return submit(op, std::forward<F>(f));
    }

    // sync_file_range(2)
    template <tcx::ioring_completion_handler F>
    auto async_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_sync_file_range(&op, fd, nbytes, offset, flags);

        return submit(op, std::forward<F>(f));
    }

    // sendmsg(2)
    template <tcx::ioring_completion_handler F>
    auto async_sendmsg(int fd, msghdr const *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_sendmsg(&op, fd, msg, flags);

        return submit(op, std::forward<F>(f));
    }

    // recvmsg(2)
    template <tcx::ioring_completion_handler F>
    auto async_recvmsg(int fd, msghdr *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recvmsg(&op, fd, msg, flags);

        return submit(op, std::forward<F>(f));
    }

    // send(2)
    template <tcx::ioring_completion_handler F>
    auto async_send(int fd, void const *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_send(&op, fd, buf, len, flags);

        return submit(op, std::forward<F>(f));
    }

    // recv(2)
    template <tcx::ioring_completion_handler F>
    auto async_recv(int fd, void *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recv(&op, fd, buf, len, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_timeout(__kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout(&op, const_cast<__kernel_timespec *>(timeout), 0, IORING_TIMEOUT_ABS * absolute);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_timeout_remove(std::uint64_t timer_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout_remove(&op, timer_id, 0);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_timeout_update(std::uint64_t timer_id, __kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        // this wil cast timer_id to a pointer which might not be 64bits
        io_uring_prep_timeout_update(&op, const_cast<__kernel_timespec *>(timeout), timer_id, IORING_TIMEOUT_ABS * absolute);
        op.addr = timer_id; // set timer_id directly

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_accept(int fd, sockaddr *addr, socklen_t *addrlen, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_accept(&op, fd, addr, addrlen, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_cancel(std::uint64_t operation_id, F &&f)
    {
        io_uring_sqe op {};
        // this wil cast operation_id to a pointer which might not be 64bits
        io_uring_prep_cancel(&op, nullptr, 0);
        op.addr = operation_id; // set operation_id directly

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_link_timeout(__kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_link_timeout(&op, const_cast<__kernel_timespec *>(timeout), IORING_TIMEOUT_ABS * absolute);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_connect(int fd, sockaddr const *addr, socklen_t len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_connect(&op, fd, addr, len);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_fallocate(int fd, int mode, off_t offset, off_t len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fallocate(&op, fd, mode, offset, len);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_fadvice(int fd, off_t offset, off_t len, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fadvise(&op, fd, offset, len, advice);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_madvice(void *addr, std::size_t length, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_madvise(&op, addr, length, advice);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_openat(int dir_fd, char const *pathname, int flags, mode_t mode, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_openat(&op, dir_fd, pathname, flags, mode);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_open(char const *pathname, int flags, mode_t mode, F &&f)
    {
        return async_openat(AT_FDCWD, pathname, flags, mode, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_openat2(int dir_fd, char const *pathname, open_how *how, std::size_t size, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_openat2(&op, dir_fd, pathname, how);
        op.len = size; // just to be safe

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_close(int fd, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_close(&op, fd);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_statx(int dir_fd, char const *pathname, int flags, unsigned mask, struct ::statx *statxbuf, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_statx(&op, dir_fd, pathname, flags, mask, statxbuf);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_read(int fd, void *buf, std::size_t len, off_t offset, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_read(&op, fd, buf, len, offset);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_write(int fd, void const *buf, std::size_t len, off_t offset, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_write(&op, fd, buf, len, offset);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_splice(int fd_in, off64_t off_in, int fd_out, off64_t *off_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_splice(&op, fd_in, off_in, fd_out, fd_out, len, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_tee(int fd_in, int fd_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_tee(&op, fd_in, fd_out, len, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_provide_buffers(void *addr, int buff_lens, int buff_count, int buff_group, int start_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_provide_buffers(&op, addr, buff_lens, buff_count, buff_group, start_id);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_remove_buffers(int buff_count, std::uint16_t buff_group, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_remove_buffers(&op, buff_count, buff_group);

        return submit(op, std::forward<F>(f));
    }

    // shutdown(2)
    template <tcx::ioring_completion_handler F>
    auto async_shutdown(int fd, int how, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_shutdown(&op, fd, how);

        return submit(op, std::forward<F>(f));
    }

    // renameat2(2)
    template <tcx::ioring_completion_handler F>
    auto async_renameat(int old_fd, char const *old_path, int new_fd, char const *new_path, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_renameat(&op, old_fd, old_path, new_fd, new_path, flags);

        return submit(op, std::forward<F>(f));
    }

    // rename(2)
    template <tcx::ioring_completion_handler F>
    auto async_rename(char const *old_path, char const *new_path, F &&f)
    {
        return async_renameat(AT_FDCWD, old_path, AT_FDCWD, new_path, 0, std::forward<F>(f));
    }

    // unlinkat2(2)
    template <tcx::ioring_completion_handler F>
    auto async_unlinkat(int dir_fd, char const *pathname, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_unlinkat(&op, dir_fd, pathname, flags);

        return submit(op, std::forward<F>(f));
    }

    // unlink(2)
    template <tcx::ioring_completion_handler F>
    auto async_unlink(char const *pathname, F &&f)
    {
        return async_unlinkat(AT_FDCWD, pathname, 0, std::forward<F>(f));
    }

    template <typename E>
    void poll(E &executor)
    {
        int consumed = io_uring_submit_and_wait(&m_uring, not m_completions.empty());
        if (consumed < 0)
            throw std::system_error(errno, std::system_category(), "io_uring_submit_and_wait");

        // std::vector<io_uring_cqe*> cqes(consumed);
        io_uring_cqe *cqes[16];
        for (unsigned count = 0; (count = io_uring_peek_batch_cqe(&m_uring, std::data(cqes), std::size(cqes)));) {
            for (std::size_t i = 0; i < count; ++i) {
                complete(executor, cqes[i]);
                io_uring_cqe_seen(&m_uring, cqes[i]);
            }
        }

        m_completions.erase(std::remove_if(m_completions.begin(), m_completions.end(), [](auto const &completion) {
            return completion.first == 0 || !static_cast<bool>(completion.second);
        }),
            m_completions.end());
    }

private:
    template <typename E>
    void complete(E &executor, io_uring_cqe *cqe)
    {
        auto it = std::lower_bound(m_completions.begin(), m_completions.end(), cqe->user_data, [](auto const &completion, std::uint64_t id) {
            return completion.first < id;
        });
        if (it != m_completions.end() && it->first == cqe->user_data) {
            executor.post([completion = std::move(it->second), result = cqe->res]() mutable {
                completion(static_cast<std::int32_t>(result));
            });
            it->first = 0;
        }
    }

public:
    ~ioring_service()
    {
        io_uring_queue_exit(&m_uring);
    }

private:
    static io_uring setup_rings(std::uint32_t entries)
    {
        io_uring uring {};
        io_uring_params params {};
        int const err_nr = io_uring_queue_init_params(entries, &uring, &params);
        if (err_nr < 0)
            throw std::system_error(err_nr, std::system_category());
        return uring;
    }

    template <typename F>
    auto submit(io_uring_sqe operation, F &&completion)
    {
        auto *sqe = io_uring_get_sqe(&m_uring);

        operation.user_data = ++m_last_id;
        *sqe = operation;

        m_completions.emplace_back(operation.user_data, std::forward<F>(completion));

        return operation.user_data;
    }

private:
    io_uring m_uring { {}, {}, {}, /*fd*/ -1, {}, {} };
    std::size_t m_last_id {};

    std::vector<std::pair<std::uint64_t, tcx::unique_function<void(std::int32_t)>>> m_completions;
};
}

#endif
