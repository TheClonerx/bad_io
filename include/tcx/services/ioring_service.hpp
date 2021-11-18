#ifndef TCX_SERVICES_IORING_SERVICE_HPP
#define TCX_SERVICES_IORING_SERVICE_HPP

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>

#include <fcntl.h> // only for AT_FDCWD

#include <liburing.h>

#include <tcx/native/handle.hpp>

namespace tcx {

/**
 * @brief Specifies that a type can be used as a completion handler in `tcx::ioring_service`
 */
template <typename F>
concept ioring_completion_handler = std::is_invocable_v<F, std::int32_t>;

/**
 * @brief This class wraps an instance of Linux's io_uring
 */
class ioring_service {
public:
    using native_handle_type = tcx::native_handle_type;

#ifdef DOXYGEN_INVOKED
    /**
     * @brief Uniquely identifies the operation in the same `tcx::ioring_service` instance
     */
    using operation_id = implementation defined;
#else
    using operation_id = decltype(std::declval<io_uring_sqe>().user_data);
#endif
    inline static native_handle_type invalid_handle = tcx::invalid_handle;

    /**
     * @brief Construct a new ioring service object
     *
     * Creates the io_uring's file descriptor and maps the rings using a default number of entries.
     */
    ioring_service();

    /**
     * @brief Construct a new ioring service object
     *
     * Creates the io_uring's file descriptor and maps the rings.
     *
     * If the value of `entries` is zero or is greater than an implementation-defined limit, that limit is used instead.
     *
     * @param entries The maximum number of entries that can be at the same time.
     */
    explicit ioring_service(std::uint32_t entries);

    ioring_service(ioring_service const &other) = delete;
    ioring_service(ioring_service &&other) = delete;

    ioring_service &operator=(ioring_service const &other) = delete;
    ioring_service &operator=(ioring_service &&other) = delete;

    /**
     * @brief Returns the underlying implementation-defined io_uring handle
     *
     * @return native_handle_type Implementation defined handle type representing the io_uring.
     */
    [[nodiscard]] native_handle_type native_handle() noexcept
    {
        return m_uring.ring_fd;
    }

    /**
     * @brief Does nothing (asynchronously)
     *
     * @param f Callback
     * @return ID of the operation
     */
    template <tcx::ioring_completion_handler F>
    operation_id async_noop(F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_nop(&op);

        return submit(op, std::forward<F>(f));
    }

    // preadv2(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_readv(int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_readv(&op, fd, iov, len, offset);
        op.rw_flags = flags;

        return submit(op, std::forward<F>(f));
    }

    // pwritev2(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_writev(int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_writev(&op, fd, iov, len, offset);
        op.rw_flags = flags;

        return submit(op, std::forward<F>(f));
    }

    // fsync(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_fsync(int fd, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fsync(&op, fd, flags);

        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_poll_add(int fd, std::uint32_t events, F &&f)
    {

        io_uring_sqe op {};
        io_uring_prep_poll_add(&op, fd, events);

        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_poll_remove(std::uint64_t operation_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_poll_remove(&op, nullptr); // this takes a pointer (which might not be 64 bits) as addr (__u64)
        op.addr = operation_id; // set the operation_id directly

        return submit(op, std::forward<F>(f));
    }

    // epoll_ctl(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_epoll_ctl(int epoll_fd, int op, int fd, epoll_event *event, F &&f)
    {
        io_uring_sqe operation {};
        io_uring_prep_epoll_ctl(&operation, epoll_fd, fd, op, event);

        return submit(op, std::forward<F>(f));
    }

    // sync_file_range(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_sync_file_range(&op, fd, nbytes, offset, flags);

        return submit(op, std::forward<F>(f));
    }

    // sendmsg(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_sendmsg(int fd, msghdr const *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_sendmsg(&op, fd, msg, flags);

        return submit(op, std::forward<F>(f));
    }

    // recvmsg(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_recvmsg(int fd, msghdr *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recvmsg(&op, fd, msg, flags);

        return submit(op, std::forward<F>(f));
    }

    // send(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_send(int fd, void const *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_send(&op, fd, buf, len, flags);

        return submit(op, std::forward<F>(f));
    }

    // recv(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_recv(int fd, void *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recv(&op, fd, buf, len, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_timeout(__kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout(&op, const_cast<__kernel_timespec *>(timeout), 0, IORING_TIMEOUT_ABS * absolute);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_timeout_remove(std::uint64_t timer_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout_remove(&op, timer_id, 0);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_timeout_update(std::uint64_t timer_id, __kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        // this wil cast timer_id to a pointer which might not be 64bits
        io_uring_prep_timeout_update(&op, const_cast<__kernel_timespec *>(timeout), timer_id, IORING_TIMEOUT_ABS * absolute);
        op.addr = timer_id; // set timer_id directly

        return submit(op, std::forward<F>(f));
    }

    // accept4(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_accept(int fd, sockaddr *addr, socklen_t *addrlen, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_accept(&op, fd, addr, addrlen, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_cancel(std::uint64_t operation_id, F &&f)
    {
        io_uring_sqe op {};
        // this wil cast operation_id to a pointer which might not be 64bits
        io_uring_prep_cancel(&op, nullptr, 0);
        op.addr = operation_id; // set operation_id directly

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_link_timeout(__kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_link_timeout(&op, const_cast<__kernel_timespec *>(timeout), IORING_TIMEOUT_ABS * absolute);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_connect(int fd, sockaddr const *addr, socklen_t len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_connect(&op, fd, addr, len);

        return submit(op, std::forward<F>(f));
    }

    // fallocate(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_fallocate(int fd, int mode, off_t offset, off_t len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fallocate(&op, fd, mode, offset, len);

        return submit(op, std::forward<F>(f));
    }

    // posix_fadvise(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_fadvice(int fd, off_t offset, off_t len, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fadvise(&op, fd, offset, len, advice);

        return submit(op, std::forward<F>(f));
    }

    // madvice(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_madvice(void *addr, std::size_t length, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_madvise(&op, addr, length, advice);

        return submit(op, std::forward<F>(f));
    }

    // openat(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_openat(int dir_fd, char const *pathname, int flags, mode_t mode, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_openat(&op, dir_fd, pathname, flags, mode);

        return submit(op, std::forward<F>(f));
    }

    // open(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_open(char const *pathname, int flags, mode_t mode, F &&f)
    {
        return async_openat(AT_FDCWD, pathname, flags, mode, std::forward<F>(f));
    }

    // openat2(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_openat2(int dir_fd, char const *pathname, ::open_how *how, std::size_t size, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_openat2(&op, dir_fd, pathname, how);
        op.len = size; // just to be safe

        return submit(op, std::forward<F>(f));
    }

    // close(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_close(int fd, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_close(&op, fd);

        return submit(op, std::forward<F>(f));
    }

    // statx(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_statx(int dir_fd, char const *pathname, int flags, unsigned mask, struct ::statx *statxbuf, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_statx(&op, dir_fd, pathname, flags, mask, statxbuf);

        return submit(op, std::forward<F>(f));
    }

    // read(2) if `offset` is less than 0, pread(2) otherwise
    template <tcx::ioring_completion_handler F>
    operation_id async_read(int fd, void *buf, std::size_t len, off_t offset, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_read(&op, fd, buf, len, offset);

        return submit(op, std::forward<F>(f));
    }

    // write(2) if `offset` is less than 0, pwrite(2) otherwise
    template <tcx::ioring_completion_handler F>
    operation_id async_write(int fd, void const *buf, std::size_t len, off_t offset, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_write(&op, fd, buf, len, offset);

        return submit(op, std::forward<F>(f));
    }

    // splice(2) use -1 to signify null offsets
    template <tcx::ioring_completion_handler F>
    operation_id async_splice(int fd_in, off64_t off_in, int fd_out, off64_t off_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_splice(&op, fd_in, off_in, fd_out, fd_out, len, flags);

        return submit(op, std::forward<F>(f));
    }

    // tee(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_tee(int fd_in, int fd_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_tee(&op, fd_in, fd_out, len, flags);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_provide_buffers(void *addr, int buff_lens, int buff_count, int buff_group, int start_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_provide_buffers(&op, addr, buff_lens, buff_count, buff_group, start_id);

        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    operation_id async_remove_buffers(int buff_count, std::uint16_t buff_group, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_remove_buffers(&op, buff_count, buff_group);

        return submit(op, std::forward<F>(f));
    }

    // shutdown(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_shutdown(int fd, int how, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_shutdown(&op, fd, how);

        return submit(op, std::forward<F>(f));
    }

    // renameat2(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_renameat(int old_fd, char const *old_path, int new_fd, char const *new_path, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_renameat(&op, old_fd, old_path, new_fd, new_path, flags);

        return submit(op, std::forward<F>(f));
    }

    // rename(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_rename(char const *old_path, char const *new_path, F &&f)
    {
        return async_renameat(AT_FDCWD, old_path, AT_FDCWD, new_path, 0, std::forward<F>(f));
    }

    // unlinkat2(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_unlinkat(int dir_fd, char const *pathname, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_unlinkat(&op, dir_fd, pathname, flags);

        return submit(op, std::forward<F>(f));
    }

    // unlink(2)
    template <tcx::ioring_completion_handler F>
    operation_id async_unlink(char const *pathname, F &&f)
    {
        return async_unlinkat(AT_FDCWD, pathname, 0, std::forward<F>(f));
    }

    void poll();

    std::size_t pending() const noexcept
    {
        return m_pending.load();
    }

    ~ioring_service();

private:
    void complete(io_uring_cqe const &cqe);

    static io_uring setup_rings(std::uint32_t entries);

    template <typename F>
    auto submit(io_uring_sqe operation, F &&completion)
    {
        struct Completion {
            void (*call)(Completion *self, std::int32_t res);
            F functor;
        };

        auto p = std::make_unique<Completion>(Completion {
            .call = +[](Completion *self, std::int32_t res) {
                try {
                    self->functor(res);
                } catch (...) {
                    delete self;
                    throw;
                }
                delete self;
            },
            .functor = std::forward<F>(completion) });

        auto *sqe = io_uring_get_sqe(&m_uring);
        if (!sqe) {
            if (int res = io_uring_submit(&m_uring); res < 0)
                throw std::system_error(-res, std::system_category(), "io_uring_submit");

            if (!(sqe = io_uring_get_sqe(&m_uring))) {
                throw std::runtime_error("io_uring is full");
            }
        }

        m_pending.fetch_add(1);

        operation.user_data = reinterpret_cast<std::uintptr_t>(p.release());
        *sqe = operation;

        return operation.user_data;
    }

private:
    io_uring m_uring { {}, {}, {}, /*fd*/ -1, {}, {} };
    std::atomic_size_t m_pending;
};
}

#endif
