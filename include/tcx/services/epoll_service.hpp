#ifndef TCX_SERVICES_EPOLL_SERVICE_HPP
#define TCX_SERVICES_EPOLL_SERVICE_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <system_error>
#include <utility>

#include <sys/epoll.h>
#include <unistd.h>

#include <tcx/native/handle.hpp>

#include <oneapi/tbb/concurrent_hash_map.h>

/** @addtogroup epoll_service Linux's epoll */

namespace tcx {

namespace impl {
    struct ErasedDeleter {

        ErasedDeleter() = default;

        template <typename T>
        ErasedDeleter(std::in_place_type_t<T>) noexcept
            : pfn_deleter(
                +[](void *ptr) {
                    delete reinterpret_cast<T *>(ptr);
                })

        {
        }

        void operator()(void *ptr) const noexcept
        {
            pfn_deleter(ptr);
        }

        void (*pfn_deleter)(void *) = nullptr;
    };
} // namespace impl

/**
 * @brief Specifies that a type can be used as a completion handler in `tcx::epoll_service`
 * @ingroup epoll_service
 */
template <typename F>
concept epoll_completion_handler = std::is_invocable_v<F, std::int32_t>;

/**
 * @ingroup epoll_service
 * @brief This class wraps an instance of Linux's epoll
 */
class epoll_service {
public:
    using native_handle_type = tcx::native::handle_type;
    inline static native_handle_type invalid_handle = tcx::native::invalid_handle;

    epoll_service()
        : m_handle(epoll_create1(EPOLL_CLOEXEC))
    {
        if (m_handle < 0)
            throw std::system_error(errno, std::system_category(), "epoll_create1");
    }

    epoll_service(epoll_service const &) = delete;
    epoll_service(epoll_service &&other) = delete;

    epoll_service &operator=(epoll_service const &) = delete;
    epoll_service &operator=(epoll_service &&other) = delete;

    /**
     * @brief Returns the underlying implementation-defined epoll handle
     *
     * @return native_handle_type Implementation defined handle type representing the epoll instance.
     */
    [[nodiscard]] native_handle_type native_handle() noexcept
    {
        return m_handle;
    }

    /**
     * @brief poll a file for events
     * @see [_man 2 epoll_ctl_](https://man.archlinux.org/man/epoll_ctl.2.en)

     * @attention
     * While epoll allows multi-shot mode, this interface only allows for one-shot mode.
     * <b>There's no way to enable multi-shot mode.</b>

     * @attention
     * epoll doesn't allow for a file to polled multiple times at a given time.
     * Trying to do so will inmediatly fail with a `EEXIST` error code.

     * @param fd file descriptor to listen events from
     * @param events a bit mask composed by ORing together zero or more of the `EPOLL*` constants
     * @param f callback
     */
    template <tcx::epoll_completion_handler F>
    void async_poll_add(int fd, std::uint32_t events, F &&f)
    {
        struct Completion {
            void (*pfn_invoke)(Completion *, int);
            int fd;
            F completion;
        };

        epoll_event event {};
        auto pf = std::make_unique<Completion>(
            +[](Completion *self, int e) {
                std::invoke(*reinterpret_cast<F *>(self->completion), e);
            },
            fd,
            std::forward<F>(f));
        event.data.ptr = pf.get();
        event.events = events | EPOLLONESHOT;
        if (epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &event) < 0)
            throw std::system_error(errno, std::system_category());

        m_data.emplace(fd, std::move(pf));
    }

    /**
     * @brief remove an existing poll request

     * @param fd file descriptor to remove
     * @throws `std::system_error(ENOENT, std::system_category())`
     */
    void poll_remove(int fd);

    std::size_t pending() const noexcept
    {
        return m_data.size();
    }

    void poll();

    ~epoll_service();

private:
    native_handle_type m_handle = invalid_handle;

    using data_map = oneapi::tbb::concurrent_hash_map<int, std::unique_ptr<void *, impl::ErasedDeleter>>;
    data_map m_data;
};

} // namespace tcx

#endif
