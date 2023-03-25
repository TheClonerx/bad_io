#ifndef TCX_SERVICES_IORING_SERVICE_HPP
#define TCX_SERVICES_IORING_SERVICE_HPP

#include <algorithm>
#include <atomic>
#include <cassert>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <system_error>
#include <type_traits>
#include <utility>

#include <fcntl.h> // only for AT_FDCWD

#include <liburing.h>

#include <tcx/allocator_aware.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/native/result.hpp>
#include <tcx/utilities/clamp.hpp>

/** @addtogroup ioring_service Linux's io_uring */

namespace tcx {

/**
 * @brief Specifies that a type can be used as a completion handler in `tcx::ioring_service`
 * @ingroup ioring_service
 */
template <typename F, typename Context>
concept ioring_completion_handler = std::is_invocable_v<F, Context &, io_uring_cqe const *>;

struct uring_context_storage;

template <typename T>
concept uring_context = std::is_base_of_v<uring_context_storage, T>;

/**
 * @brief Provides an RAII wrapper around an io_uring instance
 */
struct uring_context_storage {
    /**
     * @brief uniquely identifies the operation in the same `tcx::ioring_service` instance at a given time
     */
    enum operation_t : std::uint64_t {};

    using native_handle_type = tcx::native::handle_type;
    inline constexpr static native_handle_type invalid_handle = tcx::native::invalid_handle;

    static native::result<uring_context_storage> create(std::uint32_t entries, io_uring_params *params) noexcept
    {
        uring_context_storage result;

        int const error = io_uring_queue_init_params(entries, &result.m_uring, params);
        if (error < 0)
            return native::result<uring_context_storage>::from_error(-error);
        return native::result<uring_context_storage>::from_value(std::move(result));
    }

    static native::result<uring_context_storage> create(std::uint32_t entries, std::uint32_t flags) noexcept
    {
        io_uring_params params {};
        params.flags = flags;
        return create(entries, &params);
    }

    constexpr uring_context_storage() noexcept = default;

    /**
     * @brief Deleted copy constructor
     */
    uring_context_storage(uring_context_storage const &) = delete;

    /**
     * @brief Deleted copy assignment operator
     */
    uring_context_storage &operator=(uring_context_storage const &) = delete;

    /**
     * @brief Move constructor
     */
    uring_context_storage(uring_context_storage &&other) noexcept
        : m_uring { std::exchange(other.m_uring, default_uring()) }
    {
    }

    /**
     * @brief Move assignment operator
     *
     * Destroys the io_uring instance and takes ownership of the `other` io_uring instance.
     */
    uring_context_storage &operator=(uring_context_storage &&other) noexcept
    {
        if (this != &other && m_uring.ring_fd != invalid_handle) [[likely]] {
            io_uring_queue_exit(&m_uring);
            m_uring = std::exchange(other.m_uring, default_uring());
        }
        return *this;
    }

    ~uring_context_storage() noexcept
    {
        if (m_uring.ring_fd != invalid_handle) [[likely]] {
            io_uring_queue_exit(&m_uring);
            m_uring = default_uring();
        }
    }

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
     * @brief return the features supported by the implementation
     *
     * @see [_man 2 io_uring_setup_](https://man.archlinux.org/man/io_uring_setup.2)
     */
    [[nodiscard]] unsigned features() const noexcept
    {
        return m_uring.features;
    }

    /**
     * @brief check if the implementation supports a feature

     * this is a shortcut for `service.features() & feature != 0`
     * @see ioring_service::features()
     */
    [[nodiscard]] bool has_feature(unsigned feature) const noexcept
    {
        return m_uring.features & feature;
    }

protected:
    io_uring m_uring = default_uring();

private:
    constexpr static io_uring default_uring() noexcept
    {
        io_uring uring {};
        uring.ring_fd = uring.enter_ring_fd = invalid_handle;
        return uring;
    }
};

/**
 * @brief provides async interface for an uring_context

 */
template <typename Super>
struct uring_context_base {

    template <typename F>
    requires std::is_invocable_v<F>
    auto post(F &&f)
    {
        return async_noop([f = std::forward<F>(f)](Super &, io_uring_cqe const *) mutable {
            (void)std::invoke(f);
        });
    }

    /**
     * @brief Does nothing (asynchronously)
     *
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_noop(F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_nop(&op);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief vectored read
     * @see [_man 2 preadv2_](https://man.archlinux.org/man/preadv2.2.en)

     * Sequentially read from `fd` into the provided array of `len` io vectors `iov`, starting at `offset`

     * Buffers are processed in array order. This means that async_readv() completely fills `iov[0]` before proceeding to `iov[1]`, and so on.
     * If there is insufficient data, then not all buffers pointed to by iov may be filled.

     * If `offset` is -1 then the internal file offset is used and its advanced by the number of bytes read.
     * Otherwise the provided `offset` is used as the offset to read from, and the internal offset is not updated.

     * @param fd file descriptor
     * @param iov array of I/O vectors
     * @param iov_len numbers of I/O vectors
     * @param offset offset into the file, or -1
     * @param flags see [_man 2 pwritev2_](https://man.archlinux.org/man/preadv2.2.en#preadv2()_and_pwritev2())
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_readv(int fd, iovec const *iov, std::size_t iov_len, off64_t offset, int flags, F &&f)
    {
        // TODO: maybe change the `offset` type to `uint64_t` and use an special tag type for -1?

        // only possible values are -1, 0, or a positive integer
        assert(offset >= -1);
        assert(offset == -1 + !static_cast<Super *>(this)->has_feature(IORING_FEAT_RW_CUR_POS) == 2);

        io_uring_sqe op {};
        io_uring_prep_readv2(&op, fd, iov, tcx::utilities::clamp<unsigned>(iov_len), static_cast<uint64_t>(offset), flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief vectored write
     * @see [_man 2 pwritev2_](https://man.archlinux.org/man/pwritev2_.2.en)

     * Sequentially writes to `fd` from the provided array of `len` io vectors `iov`, starting at `offset`

     * Buffers are processed in array order. This means that async_writev()
     * writes out the entire contents of `iov[0]` before proceeding to `iov[1]`,
     * and so on.

     * If `offset` is -1 then the internal file offset is used and its advanced by the number of bytes written.
     * Otherwise the provided `offset` is used as the offset to write to, and the internal offset is not updated.

     * @param fd file descriptor
     * @param iov array of io vectors
     * @param iov_len numbers of io vectors
     * @param offset offset into the file, or -1
     * @param flags see [_man 2 pwritev2_](https://man.archlinux.org/man/pwritev2.2.en#preadv2()_and_pwritev2())
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_writev(int fd, iovec const *iov, std::size_t iov_len, off64_t offset, int flags, F &&f)
    {
        // TODO: maybe change the `offset` type to `uint64_t` and use an special tag type for -1?

        // only possible values are -1, 0, or a positive integer
        assert(offset >= -1);
        assert(offset == -1 + !static_cast<Super *>(this)->has_feature(IORING_FEAT_RW_CUR_POS) == 2);

        io_uring_sqe op {};
        io_uring_prep_writev2(&op, fd, iov, tcx::utilities::clamp<unsigned>(iov_len), static_cast<uint64_t>(offset), flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // fsync(2)
    /**
     * @brief synchronize a file's in-core state with it's underlying storage device
     * @see [_man 2 fsync_](https://man.archlinux.org/man/fsync.2.en)
     * @see [_man 2 fdatasync_](https://man.archlinux.org/man/fdatasync.2.en)

     * @param fd file descriptor
     * @param flags either 0 or a bitwise combination of the following constants:
     * - <b>`IORING_FSYNC_DATASYNC`</b>: behave like `fdatasync`
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_fsync(int fd, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fsync(&op, fd, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief poll a file for events
     * @see [_man 2 epoll_ctl_](https://man.archlinux.org/man/epoll_ctl.2.en)

     * @attention
     * This interface is only for one-shot mode.

     * @param fd file descriptor to listen events from
     * @param events a bit mask composed by ORing together zero or more of the `EPOLL*` constants
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_poll_add(int fd, std::uint32_t events, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_poll_add(&op, fd, events);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
    * @brief poll a file for events
    * @see [_man 2 epoll_ctl_](https://man.archlinux.org/man/epoll_ctl.2.en)

    * @attention
    * This interface will end up invoking `f` more than once

    * @param fd file descriptor to listen events from
    * @param events a bit mask composed by ORing together zero or more of the `EPOLL*` constants
    * @param f callback
    * @return id of the operation
    */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_poll_multishot(int fd, std::uint32_t events, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_poll_multishot(&op, fd, events);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief remove an existing poll request
     * @see tcx::ioring_service::async_cancel

     * Works like tcx::ioring_service::async_cancel except only looks for poll requests.

     * @param operation id of the polling operation to remove
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_poll_remove(uring_context_storage::operation_t operation, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_poll_remove(&op, static_cast<std::underlying_type_t<typename Super::operation_t>>(operation));
        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief Asynchronously adds, updates, or removes entries in the interest list of an epoll instance
     * @see [_man 2 epoll_ctl_](https://man.archlinux.org/man/epoll_ctl.2.en)
     * @see [_man 7 epoll_](https://man.archlinux.org/man/epoll.7.en)

     * @param epoll_fd file descriptor of the epoll instance
     * @param op operation to be performed, must be one of the folling constants:
     * - <b>EPOLL_CTL_ADD</b>: add an entry to the interest list of the epoll file descriptor,
     * - <b>EPOLL_CTL_MOD</b>: update the settings associated with `fd` in the interest list to the new settings specified in `event`.
     * - <b>EPOLL_CTL_DEL</b>: remove the target file descriptor fd from the interest list. The `event` argument is ignored and can be <b>`NULL`</b>.
     * @param fd file descriptor to perform the operations on
     * @param event events to listen to
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_epoll_ctl(int epoll_fd, int op, int fd, epoll_event const *event, F &&f)
    {
        io_uring_sqe operation {};
        io_uring_prep_epoll_ctl(&operation, epoll_fd, fd, op, const_cast<epoll_event *>(event));

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief Synchronize a file segment's in-core state with it's underlying storage device
     * @see [_man 2 sync_file_range_](https://man.archlinux.org/man/sync_file_range.2.en)

     * @warning
     * <b>This operation is extremely dangerous.</b>
     * <b>None of these operations writes out the file's metadata.</b>
     * <b>There are no guarantees that the data will be available after a crash.</b>

     * @attention unlike the `sync_file_range` syscall, which uses `off64_t` as the `nbytes`
     * argument (which is a signed 64bit integer), io_uring uses an unsigned 32bit integer.
     * Thus, using any value above UINT32_MAX is UB until io_uring adds a way to specify a larger range.

     * @param fd file descriptor
     * @param offset offset into the file
     * @param nbytes number of bytes to sync
     * @param flags see [_man 2 sync_file_range_](https://man.archlinux.org/man/sync_file_range.2.en#Some_details)
     * @param f callback
     * @return id of the operation
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_sync_file_range(int fd, uint64_t offset, uint64_t nbytes, int flags, F &&f)
    {
        assert(nbytes <= UINT32_MAX);

        io_uring_sqe op {};
        io_uring_prep_sync_file_range(&op, fd, static_cast<unsigned>(nbytes), offset, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    /**
     * @brief Transmit a message to another socket.
     * @see [_man 2 sendmsg](https://man.archlinux.org/man/sendmsg.2.en)

     * @param fd file descriptor
     * @param msg message to transmit
     * @param flags see [_man 2 sendmsg_](https://man.archlinux.org/man/sendmsg.2.en#The_flags_argument)
     * @param f callback
     */
    template <tcx::ioring_completion_handler<Super> F>
    auto async_sendmsg(int fd, msghdr const *msg, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_sendmsg(&op, fd, msg, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // recvmsg(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_recvmsg(int fd, msghdr *msg, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recvmsg(&op, fd, msg, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // send(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_send(int fd, void const *buf, std::size_t buf_len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_send(&op, fd, buf, buf_len, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // recv(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_recv(int fd, void *buf, std::size_t buf_len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_recv(&op, fd, buf, buf_len, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_timeout(__kernel_timespec const *timeout, std::uint32_t count, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout(&op, const_cast<__kernel_timespec *>(timeout), count, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_timeout_remove(uring_context_storage::operation_t timer_id, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout_remove(&op, static_cast<std::underlying_type_t<typename Super::operation_t>>(timer_id), flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_timeout_update(uring_context_storage::operation_t timer_id, __kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_timeout_update(&op, const_cast<__kernel_timespec *>(timeout), static_cast<std::underlying_type_t<typename Super::operation_t>>(timer_id), IORING_TIMEOUT_ABS * absolute);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // accept4(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_accept(int fd, sockaddr *addr, socklen_t *addr_len, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_accept(&op, fd, addr, addr_len, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_cancel(uring_context_storage::operation_t operation, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_cancel64(&op, operation, static_cast<int>(flags));

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_cancel_fd(tcx::native::handle_type fd, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_cancel_fd(&op, fd, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_link_timeout(__kernel_timespec const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_link_timeout(&op, const_cast<__kernel_timespec *>(timeout), IORING_TIMEOUT_ABS * absolute);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_connect(int fd, sockaddr const *addr, socklen_t addr_len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_connect(&op, fd, addr, addr_len);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // fallocate(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_fallocate(int fd, int mode, off_t offset, off_t len, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fallocate(&op, fd, mode, offset, len);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // posix_fadvise(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_fadvice(int fd, uint64_t offset, off_t len, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_fadvise(&op, fd, offset, len, advice);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // madvice(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_madvice(void *addr, std::size_t length, int advice, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_madvise(&op, addr, static_cast<off_t>(length), advice);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // openat(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_openat(int dir_fd, char const *pathname, int flags, mode_t mode, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_openat(&op, dir_fd, pathname, flags, mode);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // open(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_open(char const *pathname, int flags, mode_t mode, F &&f)
    {
        return async_openat(AT_FDCWD, pathname, flags, mode, std::forward<F>(f));
    }

    // openat2(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_openat2(int dir_fd, char const *pathname, ::open_how *how, std::size_t size, F &&f)
    {
        assert(size <= UINT32_MAX);

        io_uring_sqe op {};
        io_uring_prep_openat2(&op, dir_fd, pathname, how);
        op.len = static_cast<uint32_t>(size); // just to be safe

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // close(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_close(int fd, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_close(&op, fd);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // statx(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_statx(int dir_fd, char const *pathname, int flags, unsigned mask, struct ::statx *statxbuf, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_statx(&op, dir_fd, pathname, flags, mask, statxbuf);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // read(2) if `offset` is less than 0, pread(2) otherwise
    template <tcx::ioring_completion_handler<Super> F>
    auto async_read(int fd, void *buf, std::size_t buf_len, off64_t offset, F &&f)
    {
        // TODO: maybe change the `offset` type to `uint64_t` and use an special tag type for -1?

        // only possible values are -1, 0, or a positive integer
        assert(offset >= -1);
        assert(offset == -1 + !static_cast<Super *>(this)->has_feature(IORING_FEAT_RW_CUR_POS) == 2);

        io_uring_sqe op {};
        io_uring_prep_read(&op, fd, buf, tcx::utilities::clamp<unsigned>(buf_len), static_cast<uint64_t>(offset));

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // write(2) if `offset` is less than 0, pwrite(2) otherwise
    template <tcx::ioring_completion_handler<Super> F>
    auto async_write(int fd, void const *buf, std::size_t buf_len, off64_t offset, F &&f)
    {
        // TODO: maybe change the `offset` type to `uint64_t` and use an special tag type for -1?

        // only possible values are -1, 0, or a positive integer
        assert(offset >= -1);
        assert(offset == -1 + !static_cast<Super *>(this)->has_feature(IORING_FEAT_RW_CUR_POS) == 2);

        io_uring_sqe op {};
        io_uring_prep_write(&op, fd, buf, tcx::utilities::clamp<unsigned>(buf_len), static_cast<uint64_t>(offset));

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // splice(2) use -1 to signify null offsets
    template <tcx::ioring_completion_handler<Super> F>
    auto async_splice(int fd_in, off64_t off_in, int fd_out, off64_t off_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_splice(&op, fd_in, off_in, fd_out, off_out, tcx::utilities::clamp<unsigned>(len), flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // tee(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_tee(int fd_in, int fd_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_tee(&op, fd_in, fd_out, tcx::utilities::clamp<unsigned>(len), flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_provide_buffers(void *addr, int buff_lens, int buff_count, int buff_group, int start_id, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_provide_buffers(&op, addr, buff_lens, buff_count, buff_group, start_id);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler<Super> F>
    auto async_remove_buffers(int buff_count, std::uint16_t buff_group, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_remove_buffers(&op, buff_count, buff_group);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // shutdown(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_shutdown(int fd, int how, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_shutdown(&op, fd, how);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // renameat2(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_renameat(int old_fd, char const *old_path, int new_fd, char const *new_path, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_renameat(&op, old_fd, old_path, new_fd, new_path, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // rename(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_rename(char const *old_path, char const *new_path, F &&f)
    {
        return async_renameat(AT_FDCWD, old_path, AT_FDCWD, new_path, 0, std::forward<F>(f));
    }

    // unlinkat2(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_unlinkat(int dir_fd, char const *pathname, int flags, F &&f)
    {
        io_uring_sqe op {};
        io_uring_prep_unlinkat(&op, dir_fd, pathname, flags);

        return static_cast<Super *>(this)->submit(&op, std::forward<F>(f));
    }

    // unlink(2)
    template <tcx::ioring_completion_handler<Super> F>
    auto async_unlink(char const *pathname, F &&f)
    {
        return async_unlinkat(AT_FDCWD, pathname, 0, std::forward<F>(f));
    }
};

/**
 * @brief provides allocator aweraness to an uring_context
 * @tparam Allocator rebound to std::byte
 */
template <typename Super, typename Allocator>
class uring_context_allocating_base : public allocator_aware<Allocator, std::byte> {
public:
    using allocator_traits = typename allocator_aware<Allocator, std::byte>::allocator_traits;
    using allocator_type = typename allocator_aware<Allocator, std::byte>::allocator_type;

    uring_context_allocating_base(allocator_type allocator) noexcept
        : allocator_aware<Allocator, std::byte>(std::move(allocator))
    {
    }

private:
    struct ICompletion {
        virtual void invoke(Super &, io_uring_cqe const *) = 0;
        virtual ~ICompletion() = default;
    };

    template <typename Callback>
    struct Completion final : public virtual ICompletion {
        template <typename... Args>
        explicit Completion(std::in_place_t, Args &&...args)
            : callback(std::forward<Args>(args)...)
        {
        }

        void invoke(Super &service, io_uring_cqe const *result) override
        {
            if (result->flags & IORING_CQE_F_MORE) {
                // there will be more completion entries coming, do not delete
                std::invoke(this->callback, service, result);
            } else {
                // last completion,
                // ensure the pointer gets deleted even in an exception

                try {
                    std::invoke(this->callback, service, result);
                } catch (...) {
                    service.delete_object(this);
                    throw;
                }
            }
        }

        Callback callback;
        virtual ~Completion() = default;
    };

public:
    template <tcx::ioring_completion_handler<Super> F>
    native::result<uring_context_storage::operation_t> submit(io_uring_sqe *operation, F &&callback)
    {
        auto *completion = this->template new_object<Completion<F>>(std::in_place, std::forward<F>(callback));
        io_uring_sqe_set_data(operation, completion);

        if (auto const result = static_cast<Super *>(this)->submit_one(operation); result.has_error()) {
            this->delete_object(completion);
            return native::result<uring_context_storage::operation_t>::from_error(result.error());
        } else {
            return native::result<uring_context_storage::operation_t>::from_value(static_cast<uring_context_storage::operation_t>(reinterpret_cast<uintptr_t>(completion)));
        }
    }

    void complete(io_uring_cqe const *cqe)
    {
        auto udata = io_uring_cqe_get_data(cqe);
        auto *completion = reinterpret_cast<ICompletion *>(udata);
        completion->invoke(*static_cast<Super *>(this), cqe);
    }
};

template <typename Allocator = std::allocator<std::byte>>
struct unsynchronized_uring_context final
    : public uring_context_storage,
      public uring_context_allocating_base<unsynchronized_uring_context<Allocator>, Allocator>,
      public uring_context_base<unsynchronized_uring_context<Allocator>> {

    using allocator_type = typename uring_context_allocating_base<unsynchronized_uring_context<Allocator>, Allocator>::allocator_type;

private:
    unsynchronized_uring_context(uring_context_storage storage, allocator_type allocator) noexcept
        : uring_context_storage(std::move(storage))
        , uring_context_allocating_base<unsynchronized_uring_context<Allocator>, Allocator>(std::move(allocator))
    {
    }

public:
    [[nodiscard]] static tcx::native::result<unsynchronized_uring_context> create(std::uint32_t entries, io_uring_params *params, allocator_type allocator = allocator_type()) noexcept
    {
        auto storage = uring_context_storage::create(entries, params);
        if (storage.has_error()) {
            return tcx::native::result<unsynchronized_uring_context>::from_error(storage.error());
        } else {
            return tcx::native::result<unsynchronized_uring_context>::from_value(unsynchronized_uring_context(std::move(storage).value(), std::move(allocator)));
        }
    }

    [[nodiscard]] static tcx::native::result<unsynchronized_uring_context> create(std::uint32_t entries, std::uint32_t flags = 0, allocator_type allocator = allocator_type()) noexcept
    {
        io_uring_params params = {};
        params.flags = flags;
        return create(entries, &params, std::move(allocator));
    }

    [[nodiscard]] std::size_t pending() const noexcept
    {
        return m_pending;
    }

    native::result<void> submit_one(io_uring_sqe const *submission) noexcept
    {
        io_uring_sqe *sqe;
        while ((sqe = io_uring_get_sqe(&this->m_uring)) == nullptr) {
            int const result = io_uring_submit(&this->m_uring);
            switch (result) {
            case 0:
                continue;
            case EINTR:
                continue;
            case EBADR: // this is an unrecoverable error from our part
                std::abort();
            // TODO: case EAGAIN
            default:
                return tcx::native::result<void>::from_error(-result);
            }
        }
        *sqe = *submission;
        return {};
    }

    native::result<std::size_t> wait_many(std::span<io_uring_cqe *const> &completions, std::uint32_t wait_nr) noexcept
    {
        if (int const error = io_uring_submit_and_wait(&this->m_uring, wait_nr); error != 0) {
            return native::result<std::size_t>::from_error(-error);
        }

        unsigned head;
        io_uring_cqe *cqe;
        std::size_t seen = 0;
        io_uring_for_each_cqe(&this->m_uring, head, cqe)
        {
            if (seen == completions.size())
                break;
            std::size_t const shift = static_cast<bool>(this->m_uring.flags & IORING_SETUP_CQE32);
            std::memcpy(completions[seen], cqe, sizeof(*cqe) << shift);
            ++seen;
        }
        io_uring_cq_advance(&this->m_uring, seen);
        completions = std::span<io_uring_cqe *const> { completions.data(), seen };
        return native::result<std::size_t>::from_value(seen);
    }

private:
    std::size_t m_pending = 0;
};

template <typename Allocator = std::allocator<std::byte>>
struct synchronized_uring_context final
    : public uring_context_storage,
      public uring_context_allocating_base<synchronized_uring_context<Allocator>, Allocator>,
      public uring_context_base<synchronized_uring_context<Allocator>> {

    [[nodiscard]] std::size_t pending() const noexcept
    {
        return m_pending.load(std::memory_order_acquire);
    }

private:
    native::result<void> submit_to_kernel()
    {
        for (;;) {
            int const error = io_uring_submit(&this->m_uring);
            switch (error) {
            case 0:
                break;
            case EINTR:
                continue;
            case EBADR: // this is an unrecoverable error from our part
                std::abort();
            // TODO: case EAGAIN
            default:
                return tcx::native::result<void>::from_error(-error);
            }
        }
        return {};
    }

public:
    native::result<void> submit_one(io_uring_sqe const *submission) noexcept
    {
        return submit_many({ &submission, 1 });
    }

    native::result<void> submit_many(std::span<io_uring_sqe const *const> submissions) noexcept
    {
        std::unique_lock sq_lock(m_sq_mutex);
        io_uring_sqe *sqe;

        for (auto submission : submissions) {
            while ((sqe = io_uring_get_sqe(&this->m_uring)) == nullptr) {
                if (auto result = submit_to_kernel(); result.has_error())
                    return result;
            }
            std::size_t const shift = static_cast<bool>(this->m_uring.flags & IORING_SETUP_SQE128);
            std::memcpy(sqe, submission, sizeof(*submission) << shift);
        }
        return {};
    }

    native::result<void> wait_one(io_uring_cqe *completion) noexcept
    {
        return wait_many({ &completion, 1 }, 1);
    }

    native::result<void> wait_many(std::span<io_uring_cqe *const> &completions, std::uint32_t wait_nr) noexcept
    {
        // we have to separate submission and waiting due to them using a different mutex
        std::unique_lock cq_lock(m_cq_mutex);
        {
            std::unique_lock sq_lock(m_sq_mutex);
            if (auto result = submit_to_kernel(); result.has_error())
                return result;
        }

        io_uring_cqe *cqe;
        if (int const error = io_uring_wait_cqe_nr(&this->m_uring, &cqe, wait_nr); error != 0) {
            return native::result<void>::from_error(-error);
        }

        unsigned head;
        std::size_t seen = 0;
        io_uring_for_each_cqe(&this->m_uring, head, cqe)
        {
            if (seen == completions.size())
                break;
            std::size_t const shift = static_cast<bool>(this->m_uring.flags & IORING_SETUP_CQE32);
            std::memcpy(completions[seen], cqe, sizeof(*cqe) << shift);
            ++seen;
        }
        io_uring_cq_advance(&this->m_uring, seen);
        return {};
    }

private:
    std::atomic_size_t m_pending = 0;
    std::mutex m_sq_mutex;
    std::mutex m_cq_mutex;
};

} // namespace tcx

#endif
