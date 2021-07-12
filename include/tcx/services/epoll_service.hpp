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
        , m_last_id { std::exchange(other.m_last_id, 0) }
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
    auto async_poll_add(int fd, std::uint32_t events, F &&f)
    {
        epoll_event event {};
        event.data.u64 = ++m_last_id;
        event.events = events | EPOLLONESHOT;
        auto &completion = m_completions.emplace_back(static_cast<std::uint64_t>(event.data.u64), std::forward<F>(f));
        if (epoll_ctl(m_handle, EPOLL_CTL_ADD, fd, &event) < 0) {
            completion.error = errno;
        }
        return event.data.u64;
    }

    [[nodiscard]] int poll_remove(int fd) noexcept
    {
        if (epoll_ctl(m_handle, EPOLL_CTL_DEL, fd, nullptr) < 0)
            return errno;
        return 0;
    }

    template <typename E>
    void poll(E &executor, bool should_block)
    {
        if (m_completions.empty())
            return;

        sigset_t prev {};
        m_events.resize(m_completions.size());
        int result = epoll_pwait(m_handle, m_events.data(), m_events.size(), 0 - should_block, &prev);
        if (result < 0)
            throw std::system_error(errno, std::system_category(), "epoll_pwait");
        for (auto &completion : m_completions) {
            if (!completion.error)
                continue;
            executor.post([completion = std::move(completion.func), error = completion.error]() mutable {
                completion(std::move(error), 0);
            });
            completion.id = 0;
        }
        for (int i = 0; i < result; ++i) {
            auto it = std::lower_bound(m_completions.begin(), m_completions.end(), m_events[i].data.u64, [](Completion const &completion, std::uint64_t id) {
                return completion.id < id;
            });
            if (it != m_completions.end() && it->id == m_events[i].data.u64) {
                executor.post([completion = std::move(it->func), events = m_events[i].events]() mutable {
                    completion(0, std::move(events));
                });
                it->id = 0;
            }
        }
        m_completions.erase(std::remove_if(m_completions.begin(), m_completions.end(), [](Completion const &completion) {
            return completion.id == 0 || !static_cast<bool>(completion.func);
        }),
            m_completions.end());
    }

    void swap(epoll_service &other) noexcept
    {
        using std::swap;
        swap(m_handle, other.m_handle);
        swap(m_events, other.m_events);
        swap(m_completions, other.m_completions);
        swap(m_last_id, other.m_last_id);
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
        std::uint64_t id;
        tcx::unique_function<void(std::int32_t, std::uint32_t)> func;
        std::int32_t error = 0;
    };
    std::vector<Completion> m_completions;
    std::uint64_t m_last_id {};
};

}

#endif