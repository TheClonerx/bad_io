#ifndef TCX_SERVICES_IORING_SERVICE_HPP
#define TCX_SERVICES_IORING_SERVICE_HPP

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <system_error>
#include <type_traits>
#include <vector>

#include <fcntl.h> // only for AT_FDCWD
#include <linux/io_uring.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#include <tcx/unique_function.hpp>

struct timespec64;
struct msghdr;
struct sockaddr;
struct epoll_event;
struct iovec;
struct open_how;
struct statx;

namespace tcx {
template <typename F>
concept ioring_completion_handler = std::is_invocable_v<F, std::int32_t>;

class ioring_service {
public:
    using native_handle_type = int;
    static constexpr native_handle_type invalid_handle = -1;

    template <typename E>
    explicit ioring_service(E &ctx)
        : ioring_service(ctx, 1024)
    {
    }

    template <typename E>
    ioring_service(E &ctx, std::uint32_t entries)
        : ioring_service(ctx, setup_rings(entries))
    {
    }

    ioring_service(ioring_service &&other) noexcept
        : m_handle { std::exchange(other.m_handle, invalid_handle) }
        , m_ring_features { std::exchange(other.m_ring_features, 0) }
        , m_to_submit { std::exchange(other.m_to_submit, 0) }
        , m_sring_tail { std::exchange(other.m_sring_tail, nullptr) }
        , m_sring_mask { std::exchange(other.m_sring_mask, nullptr) }
        , m_sring_array { std::exchange(other.m_sring_array, nullptr) }
        , m_sqes { std::exchange(other.m_sqes, nullptr) }
        , m_cring_tail { std::exchange(other.m_cring_tail, nullptr) }
        , m_cring_head { std::exchange(other.m_cring_head, nullptr) }
        , m_cring_mask { std::exchange(other.m_cring_mask, nullptr) }
        , m_cqes { std::exchange(other.m_cqes, nullptr) }
        , m_last_id { std::exchange(other.m_last_id, 0) }
        , m_completions(std::move(other.m_completions))

    {
    }

    ioring_service &operator=(ioring_service &&other) noexcept
    {
        if (this == &other)
            return *this;
        if (m_handle != invalid_handle)
            ::close(m_handle);
        m_handle = std::exchange(other.m_handle, invalid_handle);
        m_ring_features = std::exchange(other.m_ring_features, 0);
        m_to_submit = std::exchange(other.m_to_submit, 0);
        m_sring_tail = std::exchange(other.m_sring_tail, nullptr);
        m_sring_mask = std::exchange(other.m_sring_mask, nullptr);
        m_sring_array = std::exchange(other.m_sring_array, nullptr);
        m_sqes = std::exchange(other.m_sqes, nullptr);
        m_cring_tail = std::exchange(other.m_cring_tail, nullptr);
        m_cring_head = std::exchange(other.m_cring_head, nullptr);
        m_cring_mask = std::exchange(other.m_cring_mask, nullptr);
        m_cqes = std::exchange(other.m_cqes, nullptr);
        m_last_id = std::exchange(other.m_last_id, 0);
        m_completions = std::move(other.m_completions);
        return *this;
    }

private:
    struct Setup {
        native_handle_type handle;
        std::uint32_t ring_features;

        std::uint32_t *sring_tail;
        std::uint32_t *sring_mask;
        std::uint32_t *sring_array;
        io_uring_sqe *sqes;

        std::uint32_t *cring_head;
        std::uint32_t *cring_tail;
        std::uint32_t *cring_mask;

        io_uring_cqe *cqes;
    };
    template <typename E>
    ioring_service(E &ctx, Setup info)
        : m_handle { info.handle }
        , m_ring_features { info.ring_features }
        , m_sring_tail { info.sring_tail }
        , m_sring_mask { info.sring_mask }
        , m_sring_array { info.sring_array }
        , m_sqes { info.sqes }
        , m_cring_head { info.cring_head }
        , m_cring_tail { info.cring_tail }
        , m_cring_mask { info.cring_mask }
        , m_cqes { info.cqes }
    {
    }

public:
    native_handle_type native_handle() noexcept
    {
        return m_handle;
    }

    // does nothing (asynchronously)
    template <tcx::ioring_completion_handler F>
    auto async_noop(int entry_flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_NOP;
        op.flags = entry_flags;
        return submit(op, std::forward<F>(f));
    }

    // preadv2(2)
    template <tcx::ioring_completion_handler F>
    auto async_readv(int entry_flags, int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_READV;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(iov);
        op.len = len;
        op.off = offset;
        op.rw_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // pwritev2(2)
    template <tcx::ioring_completion_handler F>
    auto async_writev(int entry_flags, int fd, iovec const *iov, std::size_t len, off_t offset, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_WRITEV;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(iov);
        op.len = len;
        op.off = offset;
        op.rw_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // fsync(2)
    template <tcx::ioring_completion_handler F>
    auto async_fsync(int entry_flags, int fd, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_FSYNC;
        op.flags = entry_flags;
        op.fd = fd;
        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    auto async_poll_add(int entry_flags, int fd, std::uint32_t events, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_POLL_ADD;
        op.flags = entry_flags;
        op.fd = fd;
#ifdef IORING_FEAT_POLL_32BITS
        if (m_ring_features & IORING_FEAT_POLL_32BITS) {
            op.poll32_events = events;
            return submit(op, [completion = std::move(f)](io_uring_cqe result) {
                completion(result.res);
            });
        }
#endif
        if (events > UINT16_MAX)
            throw std::system_error(EINVAL, std::system_category(), "no IORING_FEAT_POLL_32BITS");
        op.poll_events = events;
        return submit(op, std::forward<F>(f));
    }

    // epoll_wait(2)
    template <tcx::ioring_completion_handler F>
    auto async_poll_remove(int entry_flags, int fd, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_POLL_REMOVE;
        op.flags = entry_flags;
        op.fd = fd;
        return submit(op, std::forward<F>(f));
    }

    // epoll_ctl(2)
    template <tcx::ioring_completion_handler F>
    auto async_epoll_ctl(int entry_flags, int epoll_fd, int op, int fd, epoll_event *event, F &&f)
    {
        io_uring_sqe operation {};
        operation.opcode = IORING_OP_EPOLL_CTL;
        operation.flags = entry_flags;
        operation.fd = epoll_fd;
        operation.len = op;
        operation.addr = fd;
        operation.off = reinterpret_cast<std::uintptr_t>(event);
        return submit(op, std::forward<F>(f));
    }

    // sync_file_range(2)
    template <tcx::ioring_completion_handler F>
    auto async_sync_file_range(int entry_flags, int fd, off64_t offset, off64_t nbytes, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SYNC_FILE_RANGE;
        op.flags = entry_flags;
        op.fd = fd;
        op.off = offset;
        op.len = nbytes;
        op.sync_range_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // sendmsg(2)
    template <tcx::ioring_completion_handler F>
    auto async_sendmsg(int entry_flags, int fd, msghdr const *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SENDMSG;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(msg);
        op.msg_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // recvmsg(2)
    template <tcx::ioring_completion_handler F>
    auto async_recvmsg(int entry_flags, int fd, msghdr *msg, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_RECVMSG;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(msg);
        op.msg_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // send(2)
    template <tcx::ioring_completion_handler F>
    auto async_send(int entry_flags, int fd, void const *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SEND;
        op.flags = entry_flags;
        op.fd = fd;
        op.len = len;
        op.addr = reinterpret_cast<std::uintptr_t>(buf);
        op.msg_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // recv(2)
    template <tcx::ioring_completion_handler F>
    auto async_recv(int entry_flags, int fd, void *buf, std::size_t len, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_RECV;
        op.flags = entry_flags;
        op.fd = fd;
        op.len = len;
        op.addr = reinterpret_cast<std::uintptr_t>(buf);
        op.msg_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_timeout(int entry_flags, timespec64 const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_TIMEOUT;
        op.flags = entry_flags;
        op.addr = reinterpret_cast<std::uintptr_t>(timeout);
        op.len = 1;
        op.timeout_flags = IORING_TIMEOUT_ABS * absolute;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_timeout_remove(int entry_flags, std::uint64_t timer_id, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_TIMEOUT_REMOVE;
        op.flags = entry_flags;
        op.addr = timer_id;
        return submit(op, std::forward<F>(f));
    }

#ifdef IORING_TIMEOUT_UPDATE
    template <tcx::ioring_completion_handler F>
    auto async_timeout_update(int entry_flags, std::uint64_t timer_id, timespec64 *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_TIMEOUT_REMOVE;
        op.flags = entry_flags;
        op.addr = timer_id;
        op.addr2 = reinterpret_cast<std::uintptr_t>(timeout);
        op.timeout_flags = IORING_TIMEOUT_UPDATE | IORING_TIMEOUT_ABS * absolute;
        return submit(op, std::forward<F>(f));
    }
#endif

    template <tcx::ioring_completion_handler F>
    auto async_accept(int entry_flags, int fd, sockaddr *addr, socklen_t *addrlen, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_ACCEPT;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(addr);
        op.addr2 = reinterpret_cast<std::uintptr_t>(addrlen);
        op.accept_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_cancel(int entry_flags, std::uint64_t operation_id, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_ASYNC_CANCEL;
        op.flags = entry_flags;
        op.addr = operation_id;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_link_timeout(int entry_flags, timespec64 const *timeout, bool absolute, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_LINK_TIMEOUT;
        op.flags = entry_flags;
        op.addr = reinterpret_cast<std::uintptr_t>(timeout);
        op.len = 1;
        op.timeout_flags = IORING_TIMEOUT_ABS * absolute;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_connect(int entry_flags, int fd, sockaddr const *addr, socklen_t len, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_CONNECT;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(addr);
        op.off = len;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_fallocate(int entry_flags, int fd, int mode, off_t offset, off_t len, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_FALLOCATE;
        op.flags = entry_flags;
        op.fd = fd;
        op.len = mode;
        op.off = offset;
        op.addr = len;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_fadvice(int entry_flags, int fd, off_t offset, off_t len, int advice, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_FADVISE;
        op.flags = entry_flags;
        op.fd = fd;
        op.off = offset;
        op.addr = len;
        op.fadvise_advice = advice;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_madvice(int entry_flags, void *addr, std::size_t length, int advice, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_MADVISE;
        op.flags = entry_flags;
        op.addr = reinterpret_cast<std::uintptr_t>(addr);
        op.len = length;
        op.fadvise_advice = advice;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_openat(int entry_flags, int dirfd, char const *pathname, int flags, mode_t mode, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_OPENAT;
        op.flags = entry_flags;
        op.fd = dirfd;
        op.addr = reinterpret_cast<std::uintptr_t>(pathname);
        op.open_flags = flags;
        op.len = mode;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_open(int entry_flags, char const *pathname, int flags, mode_t mode, F &&f)
    {
        return async_openat(entry_flags, AT_FDCWD, pathname, flags, mode, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_openat2(int entry_flags, int dirfd, char const *pathname, open_how *how, std::size_t size, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_OPENAT2;
        op.flags = entry_flags;
        op.fd = dirfd;
        op.addr = reinterpret_cast<std::uintptr_t>(pathname);
        op.off = reinterpret_cast<std::uintptr_t>(how);
        op.len = size;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_close(int entry_flags, int fd, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_READ;
        op.flags = entry_flags;
        op.fd = fd;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_statx(int entry_flags, int dirfd, char const *pathname, int flags, unsigned mask, statx *statxbuf, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_STATX;
        op.flags = entry_flags;
        op.fd = dirfd;
        op.addr = reinterpret_cast<std::uintptr_t>(pathname);
        op.statx_flags = flags;
        op.len = mask;
        op.off = reinterpret_cast<std::uintptr_t>(statxbuf);
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_read(int entry_flags, int fd, void *buf, std::size_t len, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_READ;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(buf);
        op.len = len;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_write(int entry_flags, int fd, void const *buf, std::size_t len, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_WRITE;
        op.flags = entry_flags;
        op.fd = fd;
        op.addr = reinterpret_cast<std::uintptr_t>(buf);
        op.len = len;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_splice(int entry_flags, int fd_in, off64_t const *off_in, int fd_out, off64_t const *off_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SPLICE;
        op.splice_fd_in = fd_in;
        op.splice_off_in = off_in ? *off_in : -1;
        op.fd = fd_out;
        op.off = off_out ? *off_out : -1;
        op.len = len;
        op.splice_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_tee(int entry_flags, int fd_in, int fd_out, std::size_t len, unsigned flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SPLICE;
        op.splice_fd_in = fd_in;
        op.fd = fd_out;
        op.len = len;
        op.splice_flags = flags;
        return submit(op, std::forward<F>(f));
    }

#if 0
    template <tcx::ioring_completion_handler F>
    auto async_provide_buffers(int entry_flags, std::uint32_t nbuf, void* bufs, std::size_t buf_lens, std::uint16_t buf_group, std::uint16_t start_id, F&& f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_PROVIDE_BUFFERS;
        op.flags = entry_flags;
        op.fd = nbuf;
        op.addr = reinterpret_cast<std::uintptr_t>(bufs);
        op.len = reinterpret_cast<std::uintptr_t>(buf_lens);
        op.buf_group = buf_group;
        op.off = start_id;
        return submit(op, std::forward<F>(f));
    }

    template <tcx::ioring_completion_handler F>
    auto async_remove_buffers(int entry_flags, std::uint32_t nbuf, std::uint16_t buf_group,  F&& f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_REMOVE_BUFFERS;
        op.flags = entry_flags;
        op.fd = nbuf;
        op.buf_group = buf_group;
        return submit(op, std::forward<F>(f));
    }
#endif

    // shutdown(2)
    template <tcx::ioring_completion_handler F>
    auto async_shutdown(int entry_flags, int fd, int how, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_SHUTDOWN;
        op.flags = entry_flags;
        op.fd = fd;
        (void)how;
        return submit(op, std::forward<F>(f));
    }

    // renameat2(2)
    template <tcx::ioring_completion_handler F>
    auto async_renameat(int entry_flags, int old_fd, char const *old_path, int new_fd, char const *new_path, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_RENAMEAT;
        op.flags = entry_flags;
        op.fd = old_fd;
        op.addr = reinterpret_cast<std::uintptr_t>(old_path);
        op.len = new_fd;
        op.addr2 = reinterpret_cast<std::uintptr_t>(new_path);
        op.rename_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // rename(2)
    template <tcx::ioring_completion_handler F>
    auto async_rename(int entry_flags, char const *old_path, char const *new_path, F &&f)
    {
        return async_renameat(entry_flags, AT_FDCWD, old_path, AT_FDCWD, new_path, 0, std::forward<F>(f));
    }

    // unlinkat2(2)
    template <tcx::ioring_completion_handler F>
    auto async_unlinkat(int entry_flags, int dir_fd, char const *pathname, int flags, F &&f)
    {
        io_uring_sqe op {};
        op.opcode = IORING_OP_UNLINKAT;
        op.flags = entry_flags;
        op.fd = dir_fd;
        op.addr = reinterpret_cast<std::uintptr_t>(pathname);
        op.unlink_flags = flags;
        return submit(op, std::forward<F>(f));
    }

    // unlink(2)
    template <tcx::ioring_completion_handler F>
    auto async_unlink(int entry_flags, char const *pathname, F &&f)
    {
        return async_unlinkat(entry_flags, AT_FDCWD, pathname, 0, std::forward<F>(f));
    }

    template <typename E>
    void poll(E &executor, bool should_block)
    {
        sigset_t sigmask;
        int consumed = static_cast<int>(syscall(SYS_io_uring_enter, m_handle, m_to_submit, should_block, IORING_ENTER_GETEVENTS, &sigmask));
        if (consumed < 0)
            throw std::system_error(errno, std::system_category(), "io_uring_enter");

        for (;;) {
            std::uint32_t head = std::atomic_ref<std::uint32_t>(*m_cring_head).load(std::memory_order_acquire);
            std::uint32_t tail = std::atomic_ref<std::uint32_t>(*m_cring_tail).load(std::memory_order_acquire);
            if (head == tail)
                break;
            auto result = m_cqes[head & (*m_cring_mask)];
            head++;
            std::atomic_ref<std::uint32_t>(*m_cring_head).store(head, std::memory_order_release);
            auto it = std::lower_bound(m_completions.begin(), m_completions.end(), result.user_data, [](auto const &completion, std::uint64_t id) {
                return completion.first < id;
            });
            if (it != m_completions.end() && it->first == result.user_data) {
                executor.post([completion = std::move(it->second), result = result.res]() mutable {
                    completion(static_cast<std::int32_t>(result));
                });
                it->first = 0;
            }
        }
        m_completions.erase(std::remove_if(m_completions.begin(), m_completions.end(), [](auto const &completion) {
            return completion.first == 0 || !static_cast<bool>(completion.second);
        }),
            m_completions.end());
    }

    ~ioring_service()
    {
        if (m_handle != invalid_handle) {
            ::close(m_handle);
            m_handle = invalid_handle;
        }
    }

private:
    static Setup setup_rings(std::uint32_t entries)
    {

        io_uring_params params {};
        int handle = static_cast<int>(syscall(SYS_io_uring_setup, static_cast<long>(entries), &params));
        if (handle < 0)
            throw std::system_error(errno, std::system_category(), "io_uring_setup");

        std::size_t sring_sz = params.sq_off.array + params.sq_entries * sizeof(std::uint32_t);
        std::size_t cring_sz = params.cq_off.cqes + params.cq_entries * sizeof(io_uring_cqe);

        if (params.features & IORING_FEAT_SINGLE_MMAP) {
            if (cring_sz > sring_sz)
                sring_sz = cring_sz;
            cring_sz = sring_sz;
        }

        void *cq_ptr;

        /* Map in the submission and completion queue ring buffers.
         *  Kernels < 5.4 only map in the submission queue, though.
         */
        auto sq_ptr = mmap(0, sring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, handle, IORING_OFF_SQ_RING);
        if (sq_ptr == MAP_FAILED)
            throw std::system_error(errno, std::system_category(), "mmap");

        if (params.features & IORING_FEAT_SINGLE_MMAP) {
            cq_ptr = sq_ptr;
        } else {
            /* Map in the completion queue ring buffer in older kernels separately */
            cq_ptr = mmap(0, cring_sz, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, handle, IORING_OFF_CQ_RING);
            if (cq_ptr == MAP_FAILED)
                throw std::system_error(errno, std::system_category(), "mmap");
        }

        std::uint32_t *sring_tail = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(sq_ptr) + params.sq_off.tail);
        std::uint32_t *sring_mask = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(sq_ptr) + params.sq_off.ring_mask);
        std::uint32_t *sring_array = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(sq_ptr) + params.sq_off.array);

        /* Map in the submission queue entries array */
        io_uring_sqe *sqes = reinterpret_cast<io_uring_sqe *>(mmap(0, params.sq_entries * sizeof(io_uring_sqe), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, handle, IORING_OFF_SQES));
        if (sqes == MAP_FAILED)
            throw std::system_error(errno, std::system_category(), "mmap");

        std::uint32_t *cring_head = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(cq_ptr) + params.cq_off.head);
        std::uint32_t *cring_tail = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(cq_ptr) + params.cq_off.tail);
        std::uint32_t *cring_mask = reinterpret_cast<std::uint32_t *>(reinterpret_cast<char *>(cq_ptr) + params.cq_off.ring_mask);
        io_uring_cqe *cqes = reinterpret_cast<io_uring_cqe *>(reinterpret_cast<char *>(cq_ptr) + params.cq_off.cqes);

        return Setup { handle, params.features, sring_tail, sring_mask, sring_array, sqes, cring_head, cring_tail, cring_mask, cqes };
    }

    template <typename T>
    auto submit(io_uring_sqe operation, T &&completion)
    {
        operation.user_data = ++m_last_id;
        m_completions.emplace_back(operation.user_data, std::forward<T>(completion));
        m_to_submit++;

        std::uint32_t tail = *m_sring_tail;
        std::uint32_t index = tail & *m_sring_mask;
        m_sqes[index] = operation;
        m_sring_array[index] = index;
        tail++;
        std::atomic_ref<std::uint32_t>(*m_sring_tail).store(tail, std::memory_order_release);
        return operation.user_data;
    }

private:
    native_handle_type m_handle;
    std::uint32_t m_ring_features;

    std::uint32_t m_to_submit {};
    std::uint32_t *m_sring_tail;
    std::uint32_t *m_sring_mask;
    std::uint32_t *m_sring_array;
    io_uring_sqe *m_sqes;

    std::uint32_t *m_cring_tail;
    std::uint32_t *m_cring_head;
    std::uint32_t *m_cring_mask;
    io_uring_cqe *m_cqes;

    std::uint64_t m_last_id {};

    std::vector<std::pair<std::uint64_t, tcx::unique_function<void(std::int32_t)>>> m_completions;
};

}

#endif