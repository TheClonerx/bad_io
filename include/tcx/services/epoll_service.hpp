#ifndef TCX_SERVICES_EPOLL_SERVICE_HPP
#define TCX_SERVICES_EPOLL_SERVICE_HPP

#include <algorithm>
#include <cstdint>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/epoll.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <tcx/native/handle.hpp>
#include <tcx/unique_function.hpp>

namespace tcx {
template <typename F>
concept epoll_completion_handler = std::is_invocable_v<F, std::int32_t, std::uint32_t>;

class epoll_service {
public:
    using native_handle_type = tcx::native_handle_type;

    template <typename E>
    explicit epoll_service(E &)
        : m_handle(epoll_create1(EPOLL_CLOEXEC))
    {
        if (m_handle < 0)
            throw std::system_error(errno, std::system_category(), "epoll_create1");
    }

    epoll_service(epoll_service const &) = delete;
    epoll_service(epoll_service &&other) noexcept
        : m_handle { std::exchange(other.m_handle, tcx::invalid_handle) }
        , m_events(std::move(other.m_events))
        , m_completions(std::move(other.m_completions))
    {
    }

    epoll_service &operator=(epoll_service const &) = delete;
    epoll_service &operator=(epoll_service &&other) noexcept
    {
        swap(other);
        return *this;
    }

    native_handle_type native_handle() noexcept
    {
        return m_handle;
    }

    template <tcx::epoll_completion_handler F>
    void async_poll_add(int fd, std::uint32_t events, F &&f)
    {
        epoll_event event {};
        event.data.fd = fd;
        event.events = events | EPOLLONESHOT;
        auto &completion = m_completions.emplace_back(static_cast<int>(fd), 0, std::forward<F>(f));
        if (epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &event) < 0) {
            completion.error = errno;
        }
    }

    template <tcx::epoll_completion_handler F>
    void async_poll_remove(int fd, F &&f)
    {
        auto &completion = m_completions.emplace_back(-1, 0, std::forward<F>(f));
        if (epoll_ctl(m_handle, EPOLL_CTL_DEL, fd, nullptr) < 0)
            completion.error = errno;

        auto it = std::find_if(m_completions.begin(), m_completions.end(), [fd](Completion const &completion) {
            return completion.fd == fd;
        });

        // well, this should never happen unless someone added stuff using native_handle()
        if (it != m_completions.end())
            it->error = ECANCELED;
    }

    template <typename E>
    void poll(E &executor)
    {
        if (m_completions.empty())
            return;

        for (auto &completion : m_completions) {
            if (!completion.error)
                continue;
            executor.post([completion = std::move(completion.func), error = completion.error]() mutable {
                completion(std::move(error), 0);
            });
            completion.fd = -1;
        }

        sigset_t prev {};
        m_events.resize(m_completions.size());
        int result = epoll_pwait(m_handle, m_events.data(), m_events.size(), -1, &prev);
        if (result < 0)
            throw std::system_error(errno, std::system_category(), "epoll_pwait");

        for (int i = 0; i < result; ++i) {
            auto it = std::find_if(m_completions.begin(), m_completions.end(), [fd = m_events[i].data.fd](Completion const &completion) {
                return completion.fd == fd;
            });
            if (it != m_completions.end()) {
                executor.post([completion = std::move(it->func), events = m_events[i].events]() mutable {
                    completion(0, std::move(events));
                });
                it->fd = -1;
            }
        }
        m_completions.erase(std::remove_if(m_completions.begin(), m_completions.end(), [](Completion const &completion) {
            return completion.fd == -1 || !static_cast<bool>(completion.func);
        }),
            m_completions.end());
    }

    void swap(epoll_service &other) noexcept
    {
        using std::swap;
        swap(m_handle, other.m_handle);
        swap(m_events, other.m_events);
        swap(m_completions, other.m_completions);
    }

    friend void swap(epoll_service &first, epoll_service &second) noexcept
    {
        first.swap(second);
    }

    ~epoll_service()
    {
        if (m_handle != tcx::invalid_handle) {
            ::close(m_handle);
            m_handle = tcx::invalid_handle;
        }
    }

private:
    tcx::native_handle_type m_handle = tcx::invalid_handle;
    std::vector<epoll_event> m_events;
    struct Completion {
        int fd = -1;
        int error = 0;
        tcx::unique_function<void(std::int32_t, std::uint32_t)> func;
    };
    std::vector<Completion> m_completions;
};

}

#endif