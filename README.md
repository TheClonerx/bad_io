# bad_io
(the name is temporary, will change later)

This is an attempt to creating an async, event based, IO library.
Highly inspired in asio.

Currently I'm quite happy how `tcx::ioring_service` ended up being, and as of now, it should be thread-safe.

I wrote `tcx::function_view` thinking it would be useful for this project. Currently it's not, but I'm not going to let my efford go to waste.

`tcx::unique_function` was originally intended to be an allocator aware storage for callable move-only objects, but i dropped support for allocator awareness due to incrementing the complexity too much. Why not `std::function`? Well, that doesn't support move only types, which is a bummer.
Now that [`std::move_only_function`](https://en.cppreference.com/w/cpp/utility/functional/move_only_function) was added to C++23 im going to try aiming for this to be a pollyfil, but maybe other libraries like boost or abseil might do this better.

`tcx::unsynchronized_execution_context` is essentially just a function queue, and as it's name implies, it's not thread safe.

`tcx::synchronized_execution_context` is also just a function queue, but this one is thread safe. It allows calling `run()` and `post()` from multiple threads, allowing for multiple completions to be executed and posted in parallel.

Services should work with arbitrary executors. For example:
```cpp
asio::system_executor ctx;
tcx::ioring_service io_service;

tcx::async_open(ctx, io_service, "/dev/null", "rb", [&ctx, &io_service](std::error_code ec, int fd) {
    if (ec)
        throw std::system_error(ec);
    tcx::async_close(ctx. io_service, fd, tcx::detached);
});
```
this will post the completion to an [asio::system_executor](https://think-async.com/Asio/asio-1.20.0/doc/asio/reference/system_executor.html), and the lambda will be executed in an arbitrary thread.

# TODO
- [X] Implement arguments for `tcx::function_view`
- [X] Implement SBO for `tcx::unique_functiom`
- [ ] ~~Implement allocator aware move operations for `tcx::unique_function`~~
- [ ] ~~Make the executor fully allocator aware~~
- [ ] Learn about how overlapping IO works on Windows
- Implement following IO services:
    - [X] `tcx::epoll_service`
    - [X] `tcx::poll_service`
    - [ ] `tcx::overlapped_service`
    - [ ] `tcx::kqueue_service`
- [ ] Implement `tcx::thread_service` (might be redundant with `tcx::synchronized_execution_context`)
- [X] Implement `tcx::synchronized_execution_context`
- [X] How to run any coroutine inside the executors.
    (I really dont want to create a coroutine future just to use in the executors like `asio::awaitable`)

# BUILDING
Dependencies are managed using [Conan](https://conan.io/), so make sure you have it properly installed and setup.
```sh
mkdir build/
cd build;
conan install .. -u -b missing
cmake ..
cmake --build . --parallel
```

# DOCUMENTATION
Documentation can be built with doxygen using cmake,
execute the followin the build folder:
```sh
cmake --build . --target docs
firefox html/index.html
```