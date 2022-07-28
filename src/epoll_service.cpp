#include <tcx/services/epoll_service.hpp>

void tcx::epoll_service::poll_remove(int fd)
{
    if (epoll_ctl(m_handle, EPOLL_CTL_DEL, fd, nullptr) < 0)
        throw std::system_error(errno, std::system_category());

    if (data_map::accessor accessor; m_data.find(accessor, fd)) {
        struct Completion {
            void (*pfn_invoke)(Completion *self, int);
            int fd;
        };

        auto *ptr = reinterpret_cast<Completion *>(accessor->second.get());
        ptr->pfn_invoke(ptr, -ECANCELED);
        m_data.erase(accessor);
    }
}

void tcx::epoll_service::poll()
{
    sigset_t prev {};
    std::vector<epoll_event> events;
    int result = epoll_pwait(m_handle, events.data(), events.size(), -1, &prev);
    if (result < 0)
        throw std::system_error(errno, std::system_category(), "epoll_pwait");
    for (auto const &event : events) {
        struct Completion {
            void (*pfn_invoke)(Completion *self, int err_nr);
            int fd;
        };

        auto *ptr = reinterpret_cast<Completion *>(event.data.ptr);
        ptr->pfn_invoke(ptr, event.events);
        if (data_map::accessor accessor; m_data.find(accessor, ptr->fd))
            m_data.erase(accessor);
    }
}

#include <cassert>

tcx::epoll_service::~epoll_service()
{
    assert(!pending() && "tried to destroy an epoll instance with pending operations");
    if (m_handle != invalid_handle) {
        ::close(m_handle);
        m_handle = invalid_handle;
    }
}