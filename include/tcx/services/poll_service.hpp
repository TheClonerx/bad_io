#ifndef TCX_POLL_SERVICE_HPP
#define TCX_POLL_SERVICE_HPP

#include "tcx/unique_function.hpp"
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include <poll.h>

namespace tcx {

class poll_service {

public:
    template <typename F>
    void async_poll_add(int fd, short events, F &&f)
    {
        auto it = std::lower_bound(m_fds.cbegin(), m_fds.cend(), fd, [](pollfd const &entry, int fd) { return entry.fd < fd; });
        if (it != m_fds.cend() && it->fd == fd)
            throw std::runtime_error("already polling this file descriptor");
        auto const index = m_fds.cend() - it;
        m_fds.insert(it, pollfd { .fd = fd, .events = events });
        m_completions.insert(m_completions.cbegin() + index, std::forward<F>(f));
    }

    void poll()
    {
        auto fds = m_fds;
        int res = ::poll(fds.data(), fds.size(), -1);

        if (res < 0)
            throw std::system_error(errno, std::system_category());

        for (int i = 0; i < res; ++i) {
            auto it = std::lower_bound(m_fds.cbegin(), m_fds.cend(), fds[i].fd, [](pollfd const &entry, int fd) { return entry.fd < fd; });
            auto const index = m_fds.cend() - it;
            m_fds.erase(it);
            auto f = std::move(m_completions[i]);
            m_completions.erase(m_completions.cbegin() + index);
            f(std::move(fds[i].revents));
        }
    }

public:
    std::vector<pollfd> m_fds;
    std::vector<tcx::unique_function<void(short)>> m_completions;
};

} // namespace tcx

#endif