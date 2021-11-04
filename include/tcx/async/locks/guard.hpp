#ifndef TCX_ASYNC_LOCKS_GUARD_HPP
#define TCX_ASYNC_LOCKS_GUARD_HPP

#include <utility>
namespace tcx {

struct defer_lock_t {
    explicit defer_lock_t() = default;
};

struct try_to_lock_t {
    explicit try_to_lock_t() = default;
};

struct adopt_lock_t {
    explicit adopt_lock_t() = default;
};

inline constexpr auto defer_lock = defer_lock_t {};
inline constexpr auto try_to_lock = try_to_lock_t {};
inline constexpr auto adopt_lock = adopt_lock_t {};

template <typename S>
struct unique_lock {
public:
    using semaphore_type = S;

    constexpr unique_lock() noexcept = default;

    // these constructors semantically are different but in the end are the same

    unique_lock(semaphore_type &s, tcx::adopt_lock_t) noexcept
        : m_sem { &s }
        , m_locked { true }
    {
    }

    unique_lock(semaphore_type &s, tcx::try_to_lock_t) noexcept
        : m_sem { &s }
        , m_locked { s.try_acquire() }
    {
    }

    unique_lock(semaphore_type &s, tcx::defer_lock_t) noexcept
        : m_sem { &s }
        , m_locked { false }
    {
    }

    semaphore_type *semaphore() const noexcept
    {
        return m_sem;
    }

    semaphore_type *release() noexcept
    {
        return std::exchange(m_sem, nullptr);
    }

    bool owns_lock() const noexcept
    {
        return m_sem != nullptr && m_locked;
    }

    explicit operator bool() const noexcept
    {
        return owns_lock();
    }

    void unlock()
    {
        if (owns_lock()) {
            m_sem->release();
            m_sem = nullptr;
        }
    }

    ~unique_lock()
    {
        unlock();
    }

private:
    semaphore_type *m_sem = nullptr;
    bool m_locked = false;
};

}

#endif