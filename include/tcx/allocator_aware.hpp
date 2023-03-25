#ifndef TCX_ALLOCATOR_AWARE_HPP
#define TCX_ALLOCATOR_AWARE_HPP

#include <cstddef>
#include <memory>
#include <utility>

namespace tcx {

template <typename Allocator, typename DesiredType = typename std::allocator_traits<Allocator>::value_type>
struct allocator_aware : private std::allocator_traits<Allocator>::template rebind_alloc<DesiredType> {
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<DesiredType>;
    using allocator_traits = typename std::allocator_traits<Allocator>::template rebind_traits<DesiredType>;

    constexpr allocator_aware(allocator_type allocator)
        : allocator_type(std::move(allocator))
    {
    }

    constexpr allocator_type get_allocator() const noexcept
    {
        auto copy = *static_cast<allocator_type const *>(this);
        return copy;
    }

protected:
    template <typename U>
    [[nodiscard]] U *allocate_obect(std::size_t n = 1)
    {
        using alloc_t = typename allocator_traits::template rebind_alloc<U>;
        using traits_t = typename allocator_traits::template rebind_traits<U>;

        auto alloc = alloc_t(get_allocator());
        return traits_t::allocate(alloc, n);
    }

    template <typename U>
    void deallocate_object(U *ptr, std::size_t n = 0)
    {
        using alloc_t = typename allocator_traits::template rebind_alloc<U>;
        using traits_t = typename allocator_traits::template rebind_traits<U>;

        auto alloc = alloc_t(get_allocator());
        return traits_t::deallocate(alloc, ptr, n);
    }

    template <typename U, typename... Args>
    [[nodiscard]] U *new_object(Args &&...args)
    {
        using alloc_t = typename allocator_traits::template rebind_alloc<U>;
        using traits_t = typename allocator_traits::template rebind_traits<U>;

        auto alloc = alloc_t(get_allocator());
        auto ptr = allocate_obect<U>();
        try {
            traits_t::construct(alloc, ptr, std::forward<Args>(args)...);
        } catch (...) {
            deallocate_object(ptr);
            throw;
        }
        return ptr;
    }

    template <typename U>
    void delete_object(U *ptr) noexcept
    {
        using alloc_t = typename allocator_traits::template rebind_alloc<U>;
        using traits_t = typename allocator_traits::template rebind_traits<U>;

        auto alloc = alloc_t(get_allocator());
        traits_t::destroy(alloc, ptr);
        deallocate_object(ptr);
    }
};

} // namespace tcx

#endif
