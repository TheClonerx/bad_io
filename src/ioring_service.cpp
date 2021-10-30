#include <tcx/services/ioring_service.hpp>

#include <cassert>

tcx::ioring_service::ioring_service(std::uint32_t entries)
    : m_uring(setup_rings(entries))
{
}

tcx::ioring_service::ioring_service()
    : ioring_service(1024)
{
}

tcx::ioring_service::~ioring_service()
{
    // this will leak pending completions!
    assert(!pending() && "tried to destroy an io_uring instance with pending operations");
    io_uring_queue_exit(&m_uring);
}

io_uring tcx::ioring_service::setup_rings(std::uint32_t entries)
{
    io_uring uring {};
    io_uring_params params {};
    params.flags = IORING_SETUP_CLAMP;
    int const err_nr = io_uring_queue_init_params(entries, &uring, &params);
    if (err_nr < 0)
        throw std::system_error(-err_nr, std::system_category());
    return uring;
}

void tcx::ioring_service::complete(io_uring_cqe const &cqe)
{
    struct Completion {
        void (*call)(Completion *self, std::int32_t res);
    };

    auto *p = reinterpret_cast<Completion *>(cqe.user_data);
    m_pending.fetch_sub(1);
    p->call(p, cqe.res);
}

void tcx::ioring_service::poll()
{
    // io_uring actually allows waiting with no pending operations, so you can submit in one thread and wait in another

    int consumed = io_uring_submit_and_wait(&m_uring, 1);
    if (consumed < 0)
        throw std::system_error(-consumed, std::system_category(), "io_uring_submit_and_wait");

    for (;;) {
        io_uring_cqe *cqe = nullptr;
        int err_nr = io_uring_peek_cqe(&m_uring, &cqe);
        if (err_nr < 0 && err_nr != -EAGAIN)
            throw std::system_error(-err_nr, std::system_category(), "io_uring_peek_cqe");

        if (cqe == nullptr)
            return;

        auto const cqe_s = *cqe;
        io_uring_cqe_seen(&m_uring, cqe);
        complete(cqe_s);
    }
}
