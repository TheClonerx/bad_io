# bad_io
(the name is temporary, will change later)

This is an attempt to creating an async, event based, IO.
Highly inspired in asio.

Currently I'm quite happy how `tcx::ioring_service` ended up being, however its not perfect; and it's not thread safe.

I wrote `tcx::function_view` thinking it would be useful for this project. Currently it's not, but I'm not going to let my efford go to waste.

`tcx::unique_function` was originally intended to be an allocator aware storage for callable move-only object, but i dropped support for allocator awareness due to incrementing the complexity too much. Why not `std::function`? Well, that doesn't support move only types, which is a bummer.
Now that `std::move_only_function` was added to C++23 im going to try aiming for this to be a pollyfil, but maybe other libraries like boost or abseil will do this better.

`tcx::unsynchronized_execution_context` is essentially just a function queue, and as it's name implies, it's not thread safe.

`tcx::synchronized_execution_context` is also just a function queue, but wrapped in a mutex. It allows calling `run()` and `post()` from multiple threads, allowing for multiple completions to be executed and posted in pararllel.
A mutex wrapped `std::queue` is indeed not the best solution, but it's preferable that to a UB filled concurrent queue manually written by me. (maybe I should use [cameron314's concurrentqueue](https://github.com/cameron314/concurrentqueue)?)

# TODO
- [X] Implement arguments for `tcx::function_view`
- [X] Implement SBO for `tcx::unique_functiom`
- ~~[ ] Implement allocator aware move operations for `tcx::unique_function`~~
- ~~[ ] Make the executor fully allocator aware~~
- [ ] Learn about how overlapping IO works on Windows
- Implement following IO services:
    - [X] `tcx::epoll_service`
    - [ ] `tcx::poll_service`
    - [ ] `tcx::overlapped_service`
    - [ ] and maybe `tcx::kqueue_service`
- [ ] Implement `tcx::thread_service` (might be redundant with `tcx::synchronized_execution_context`)
- [X] Implement `tcx::synchronized_execution_context`
- [ ] How to run any coroutine inside the executors.
    (I really dont want to create a coroutine future just to use in the executors like `asio::awaitable`)


# DOCUMENTATION
## `tcx::epoll_service`
`#include <tcx/services/epoll_service.hpp>`

### Type members
#### `typename native_handle_type` = `int`

### Function members
#### `explicit epoll_service(E &context)` (constructor)
Creates the epoll file descriptor.

#### `epoll_service(epoll_service const&) = delete` (constructor)
#### `epoll_service(epoll_service &&) noexcept` (constructor)
Moves the epoll_service

#### `~epoll_service()` (destructor)
Closes the epoll file descriptor.

#### `epoll_service &operator=(epoll_service const&) = delete`
#### `epoll_service &operator=(epoll_service &&) noexcept`
Moves the epoll_service.

#### `native_handle_type native_handle() noexcept`
Returns the native handle to the epoll instance.

#### `void poll(E &executor, bool should_block)`
Optionally waits for the completion of an operation if `should_block` is true, and then it posts the completions (if any) to `executor`.

#### `void swap(epoll_service &other) noexcept`
Swaps two `tcx::epoll_service`.

#### `friend void swap(epoll_service &first, epoll_service &second) noexcept`
Swaps two `tcx::epoll_service`.

#### `void async_poll_add(int fd, std::uint32_t events, F &&f)`
Adds an entry to the interest list of the epoll instance, similar to polling using `epoll(7)`, however, it always works in one shot mode.  
`F` must be callable with the following function signature `void(int, std::uint32_t)`, the first argument, if non zero, corresponds to an error (`errno(3)`), the second argument is a bit mask of the triggered events.  

#### `void poll_remove(int fd, F &&f) noexcept`
Removes the target file descriptor `fd` from the interest list of the epoll instance.  
`F` must be callable with the following function signature `void(int, std::uint32_t)`, the first argument, if non zero, corresponds to an error (`errno(3)`), the second argument is always zero.  
The associated poll request will complete with an error of `ECANCELED`.

#
#

## `tcx::ioring_service`
`#include <tcx/services/ioring_service.hpp>`

### Type members
#### `typename native_handle_type` = `int`
The native handle type for the io_uring instance.

### Function members
#### `ioring_service()` (constructor)
Creates the io_uring's file descriptor and maps the rings using a default number of entries.

#### `explicit ioring_service(std::uint32_t entries)` (constructor)
Creates the io_uring's file descriptor and maps the rings. Up to `entries` can be submitted at the same time.

#### `ioring_service(ioring_service const&) = delete` (constructor)

#### `ioring_service(ioring_service &&) noexcept` (constructor)
Moves the ioring_service.

#### `~ioring_service()` (destructor)
Closes the io_uring's file descriptor and unmaps the rings.

#### `ioring_service &operator=(ioring_service const&) = delete`

#### `ioring_service &operator=(ioring_service &&) noexcept`
Moves the ioring_service.

#### `native_handle_type native_handle() noexcept`
Returns the native handle to the io_uring instance.

#### `void poll(E &executor)`
Submits the operations to the io_uring instance, then it waits for the completion of an operation, and finally it posts the completions (if any) to `executor`.

#
For the next `async_` functions, the following always hold true:

- `F` must be callable with the following function signature `void(std::int32_t)`, the argument corresponds to the `res` field of the `io_uring_cqe` structure.
- Returns the id of the operation, this uniquely identifies the operation in the same `tcx::ioring_service` instance.

For more information on `io_uring_sqe`, `io_uring_cqe` and the operations please read the man page `io_uring_enter(2)`.
#

#### `auto async_noop(F &&f)`
Does nothing (asynchronously).

#### `auto async_readv(int fd, iovec const* iov, std::size_t len, off_t offset, int flags, F&& f)`
Vectored read, similar to the `preadv2` system call.

#### `auto async_writev(int fd, iovec const* iov, std::size_t len, off_t offset, int flags, F&& f)`
Vectored write, similar to the `pwritev2` system call.

#### `auto async_fsync(int fd, F&& f)`
File sync, similar to the `fsync` system call.

#### `auto async_poll_add(int fd, std::uint32_t events, F&& f)`
Poll the file `fd` for the events specified in `events`, similar to polling using `epoll`, however, it always works in one shot mode.  
The completion event result is the returned mask of events. 

#### `auto async_poll_remove(std::uint64_t operation_id, F&& f)`
Remove an existing poll request.
`operation_id` must be the result from a call to `async_poll_add`.
The completion event result is zero if found, otherwise `-ENOENT`.

#### `auto async_epoll_ctl(int epoll_fd, int op, int fd, epoll_event* event, F&& f)`
Add, remove or modify entries in the interest list of the epoll instance `fd`, similar to the `epoll_ctl` system call.

#### `auto async_sync_file_range(int fd, off64_t offset, off64_t nbytes, unsigned flags, F&& f)`
Sync a file range, similar to the `sync_file_range` system call. 

#### `auto async_sendmsg(int fd, msghdr const* msg, int flags, F&& f)`
Send a message, simiar to the `sendmsg` system call.

#### `auto async_recvmsg(int fd, msghdr* msg, int flags, F&& f)`
Receive a message, simiar to the `recvmsg` system call.

#### `auto async_send(int fd, void const* buf, std::size_t len, int flags, F&& f)`
Send bytes, similar to the `send` system call.

#### `auto async_recv(int fd, void* buf, std::size_t len, int flags, F&& f)`
Receive bytes, similar to the `recv` system call.

#### `auto async_timeout(__kernel_timespec const* timeout, bool absolute, F&& f)`
Register a timeout.  
The completion event result is `-ETIME`, or `-ECANCELED` if it got cancelled.

#### `auto async_timeout_remove(std::uint64_t timer_id, F&& f)`
Removes a timeout. `timer_id` must be a number returned by `async_timeout`.
The completion event result is zero if the timeout was found and cancelled succefully, `-EBUSY` if the timeout was found but it has already completed, or `-ENOENT` if the timeout wasn't found.

#### `auto async_timeout_update(std::uint64_t timer_id, timespec64* timeout, bool absolute, F&& f)`
Updates a timeout. `timer_id` must be a number returned by `async_timeout`.
The completion event result is zero if the timeout was found and updated succefully, `-EBUSY` if the timeout was found but it has already completed, or `-ENOENT` if the timeout wasn't found.

#### `auto async_accept(int fd, sockaddr* addr, socklen_t* addrlen, int flags, F&& f)`
Accept a connection, similar to the `accept4` system call.

#### `auto async_cancel(std::uint64_t operation_id, F&& f)`
Attempt to cancel an operation. `operation_id` must be a number returned by one of the `async_` functions. 
The completion event result is zero if the operation was found, `-ENOENT` if not found, or `-EALREADY` if the operation was already attempted cancelled.

#### `auto async_link_timeout(timespec64 const* timeout, bool absolute, F&& f)`
_**This operation must be linked with another operation using the `IOSQE_IO_LINK` entry flag.**_  
Link a timeout to an operation, the linked operation will be attempt cancelled if the timeout fires.
The completion event result is `-ETIME` if the linked operation was attempt cancelled, or `-ECANCELLED` if the linked request completed before the timeout.

#### `auto async_connect(int fd, sockaddr const* addr, socklen_t len, F&& f)`
Connect a socket, similar the `connect` system call.

#### `auto async_fallocate(int fd, int mode, off_t offset, off_t len, F&& f)`
File allocate, similar to the `fallocate` system call.

#### `auto async_fadvice(int fd, off_t offset, off_t len, int advice, F&& f)`
File advice, similar to the `posix_fadvice` system call.

#### `auto async_madvice(void* addr, std::size_t length, int advice, F&& f)`
Memory advice, similar to the `madvice` system call.

#### `auto async_openat(int dir_fd, char const* pathname, int flags, mode_t mode, F&& f)`
Open a file, similar to the `openat` system call.

#### `auto async_open(char const* pathname, int flags, mode_t mode, F&& f)`
It's just `async_openat` but with `AT_FDCWD` as the file descriptor, similar to the `open` system call.

#### `auto async_openat2(int dirfd, char const* pathname, open_how* how, std::size_t size, F&& f)`
Open a file, similar to the `openat2` system call.

#### `auto async_close(int fd, F&& f)`
Close a file, similar to the `close` system call.

#### `auto async_statx(int dir_fd, char const* pathname, int flags, unsigned mask, statx* statxbuf, F&& f)`
Stat a file, similar to the `statx` system call.

#### `auto async_read(int fd, void* buf, std::size_t len, F&& f)`
Read from a file, similar to the `read` system call.

#### `auto async_write(int fd, void const* buf, std::size_t len, F&& f)`
Write to a file, similar to the `read` system call.

#### `auto async_splice(int fd_in, off64_t off_in, int fd_out, off_out, std::size_t len, unsigned flags, F&& f)`
Splice data to/from a pipe, similar to the `splice` system call.

#### `auto async_tee(int fd_in,  int fd_out,  std::size_t len, unsigned flags, F&& f)`
***Both file descriptors must refer to a pipe.***  
Duplicating pipe content, similar to the `tee` system call.

#### `auto async_provide_buffers(void *addr, int buff_lens, int buff_count, int buff_group, int start_id, F &&f)`
TODO: doc

#### `auto async_remove_buffers(int buff_count, std::uint16_t buff_group, F &&f)`
TODO: doc

#### `auto async_shutdown(int fd, int how, F&& f)`
Shut down part of a full-duplex connection, similar to the `shutdown` system call.

#### `auto async_renameat(int old_fd, char const* old_path, int new_fd, char const* new_path, int flags, F&& f)`
Rename or move a file, similar to the `renameat2` system call.

#### `auto async_rename(char const* old_path, char const* new_path, F&& f)`
It's just `async_renameat` but with `AT_FDCWD` as the file descriptors, similar to the `rename` system call.

#### `auto async_unlinkat(int dir_fd, char const* pathname, int flags, F&& f)`
Remove a file, similar to the `unlinkat2` system call.

#### `auto async_unlink(char const* pathname, F&& f)`
It's just `async_unlinkat` but with `AT_FDCWD` as the file descriptor, similar to the `unlink` system call.
