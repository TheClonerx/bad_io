#ifndef TCX_UNIQUE_FUNCTION_HPP
#define TCX_UNIQUE_FUNCTION_HPP

#include <memory>
#include <type_traits>
#include <utility>

namespace tcx {
template <typename F, typename Allocator = std::allocator<std::byte>>
class unique_function;

template <typename R, typename... Args, typename Allocator>
class unique_function<R(Args...), Allocator> {
public:
    using allocator_type = Allocator;
    using result_type = R;

private:
    using func_impl_t = result_type (*)(unique_function *self, Args &&...);
    using func_dest_t = void (*)(unique_function *self);
    using func_move_t = void (*)(unique_function *self, unique_function *other);
    using void_pointer = typename std::allocator_traits<allocator_type>::void_pointer;

public:
    unique_function() noexcept = default;

    template <typename F>
    unique_function(std::allocator_arg_t, allocator_type const &alloc, F &&f) noexcept(std::is_convertible_v<std::remove_cvref_t<F>, result_type (*)(Args...)>) requires(std::is_invocable_r_v<R, F, Args...>)
        : m_alloc(alloc)
    {
        if constexpr (std::is_convertible_v<std::remove_cvref_t<F>, result_type (*)(Args...)>) { // a function pointer/reference or a stateless lambda was passed
            m_vptr = reinterpret_cast<void *>(static_cast<result_type (*)(Args...)>(f));
            m_impl = +[](unique_function *self, Args &&...args) -> result_type {
                auto f = reinterpret_cast<std::decay_t<F>>(self->m_vptr);
                return f(std::forward<Args>(args)...);
            };
        } else {
            void *p = m_storage;
            size_t space = sizeof(m_storage);
            p = std::align(alignof(F), sizeof(F), p, space);
            if (p) { // we can store the object inline at p
                std::construct_at(reinterpret_cast<std::remove_cvref_t<F> *>(p), std::forward<F>(f));
                func_dest_t dest = +[](unique_function *self) -> void {
                    auto p = reinterpret_cast<std::remove_cvref_t<F> *>(self->m_vptr);
                    std::destroy_at(p);
                    self->m_vptr = nullptr;
                };
                func_impl_t impl = +[](unique_function *self, Args &&...args) -> result_type {
                    auto *f = reinterpret_cast<std::remove_cvref_t<F> *>(self->m_vptr);
                    return (*f)(std::forward<Args>(args)...);
                };
                func_move_t mov = +[](unique_function *lhs, unique_function *rhs) {
                    // TODO
                };
                m_dest = dest;
                m_impl = impl;
                m_vptr = p;

            } else {
                using rebound_allocator_t = typename std::allocator_traits<allocator_type>::template rebind_alloc<std::remove_cvref_t<F>>;

                rebound_allocator_t rebound_allocator = m_alloc;
                auto data = std::allocator_traits<rebound_allocator_t>::allocate(rebound_allocator, 1);
                std::allocator_traits<rebound_allocator_t>::construct(rebound_allocator, data, std::forward<F>(f));

                func_dest_t dest = +[](unique_function *self) -> void {
                    rebound_allocator_t rebound_allocator = self->m_alloc;
                    auto p = static_cast<typename std::allocator_traits<rebound_allocator_t>::pointer>(std::move(self->m_data));
                    std::allocator_traits<rebound_allocator_t>::destroy(rebound_allocator, p);
                    std::allocator_traits<rebound_allocator_t>::deallocate(rebound_allocator, p, 1);

                    // change the active member of the union
                    std::destroy_at(std::addressof(self->m_data));
                    self->m_vptr = nullptr;
                };
                func_impl_t impl = +[](unique_function *self, Args &&...args) -> result_type {
                    auto *f = reinterpret_cast<std::remove_cvref_t<F> *>(std::to_address(self->m_data));
                    return (*f)(std::forward<Args>(args)...);
                };
                func_move_t mov = +[](unique_function *lhs, unique_function *rhs) {
                    // TODO
                };
                m_dest = dest;
                m_impl = impl;
                m_data = data;
            }
        }
    }

    template <typename F>
    unique_function(F &&f)
        : unique_function(std::allocator_arg, allocator_type {}, std::forward<F>(f))
    {
    }

    unique_function(unique_function const &) = delete;
    unique_function(unique_function &&other) noexcept
        : m_alloc(std::move(other.m_alloc))
        , m_impl(std::exchange(other.m_impl, nullptr))
        , m_dest(std::exchange(other.m_dest, nullptr))
    {
        if (m_dest)
            m_data = std::exchange(other.m_data, nullptr);
        else
            m_vptr = std::exchange(other.m_vptr, nullptr);
    }

    unique_function &operator=(unique_function const &) = delete;
    unique_function &operator=(unique_function &&other)
    {
        swap(*this, other);
        return *this;
    }

    // having allocator aware move, or using small buffer optimization
    // would require storing a function pointer to the move operation.
    // while moving using a different allocator is not a common operation,
    // having small buffer optimization would be great for most cases

    template <typename... U>
    result_type operator()(U &&...args) requires(std::is_invocable_v<func_impl_t, unique_function *, U...>)
    {
        return m_impl(this, std::forward<U>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return m_impl;
    }

    friend void swap(unique_function &first, unique_function &second) noexcept
    {
        using std::swap;
        if (first.m_dest && second.m_dest) {
            swap(first.m_data, second.m_data);
        } else if (first.m_dest) {
            auto tmp = std::move(first.m_data);
            std::destroy_at(std::addressof(first.m_data));
            first.m_vptr = second.m_vptr;
            std::construct_at(std::addressof(second.m_data), std::move(tmp));
        } else {
            auto tmp = std::move(second.m_data);
            std::destroy_at(std::addressof(second.m_data));
            second.m_vptr = first.m_vptr;
            std::construct_at(std::addressof(first.m_data), std::move(tmp));
        }
        swap(first.m_alloc, second.m_alloc);
        swap(first.m_impl, second.m_impl);
        swap(first.m_dest, second.m_dest);
        swap(first.m_move, second.m_move);
        swap(first.m_storage, second.m_storage);
    }

    ~unique_function()
    {
        if (m_dest)
            m_dest(this);
        m_data = nullptr;
        m_dest = nullptr;
        m_impl = nullptr;
        m_move = nullptr;
    }

private:
    [[no_unique_address]] allocator_type m_alloc;
    func_impl_t m_impl = nullptr;
    func_dest_t m_dest = nullptr;
    func_move_t m_move = nullptr; // supposed to store the move operation
    union {
        void_pointer m_data;
        void *m_vptr = nullptr;
    };
    struct _state {
        [[no_unique_address]] allocator_type m_alloc;
        func_impl_t m_impl = nullptr;
        func_dest_t m_dest = nullptr;
        func_move_t m_move = nullptr;
        union {
            void_pointer m_data;
            void *m_vptr = nullptr;
        };
    };
    char m_storage[64 - sizeof(_state)];
};

}
#endif