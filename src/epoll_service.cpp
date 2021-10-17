#include <tcx/services/epoll_service.hpp>

void tcx::epoll_service::poll()
{
    if (m_completions.empty())
        return;

    for (auto &completion : m_completions) {
        if (!completion.error)
            continue;
        completion.fd = -1;
        auto f = std::move(completion.func);
        f(std::move(completion.error), 0);
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
            it->fd = -1;
            auto f = std::move(it->func);
            f(std::move(it->error), 0);
        }
    }
    std::erase_if(m_completions, [](Completion const &completion) {
        return completion.fd == -1;
    });
}

tcx::epoll_service::~epoll_service()
{
    if (m_handle != tcx::invalid_handle) {
        ::close(m_handle);
        m_handle = tcx::invalid_handle;
    }
}