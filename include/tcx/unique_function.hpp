#ifndef TCX_UNIQUE_FUNCTION_HPP
#define TCX_UNIQUE_FUNCTION_HPP

#include <cstddef> // std::nullptr_t std::size_t
#include <functional> // std::invoke std::reference_wrapper
#include <memory> // std::make_unique
#include <type_traits> // std::is_invocable
#include <utility> // std::move std::forward std::swap

namespace tcx {

template <typename F>
class unique_function;

template <typename R, typename... Args>
class alignas(64) unique_function<R(Args...)> {
public:
    using result_type = R;
    using arguments_tuple = std::tuple<Args...>;

private:
    using func_call_t = result_type (*)(unique_function *self, Args &&...);
    using func_destroy_t = void (*)(void *data) noexcept;
    using func_move_t = void (*)(void *lhs, void *rhs) noexcept;

    struct virtual_table {
        func_move_t move_construct = nullptr;
        func_destroy_t destroy = nullptr;
    };

    struct virtual_state {
        virtual_table const *vtable = nullptr;
        func_call_t call = nullptr; // this has been moved out of vtable to avoid a pointer indirection
        void *data = nullptr;
    };

public:
    unique_function() noexcept = default;
    unique_function(std::nullptr_t) noexcept
        : unique_function()
    {
    }

    template <typename F>
    explicit unique_function(std::reference_wrapper<F> ref) noexcept
        requires std::is_invocable_v<F, Args...>
        : m_state {
            .vtable = nullptr,
            .call = +[](unique_function *self, Args &&...args) -> result_type {
                auto &ref = *reinterpret_cast<typename F::type *>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    return (void)std::invoke(ref, std::forward<Args>(args)...);
                else
                    return std::invoke(ref, std::forward<Args>(args)...);
            },
            .data = &ref.get()
        }
    {
    }

    template <typename F>
    explicit unique_function(F &&f) noexcept
        requires(std::is_convertible_v<std::remove_cvref_t<F>, result_type (*)(Args...)>)
        : m_state {
            .vtable = nullptr,
            .call = +[](unique_function *self, Args &&...args) -> result_type {
                auto *pfn = reinterpret_cast<result_type (*)(Args...)>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    return (void)std::invoke(pfn, std::forward<Args>(args)...);
                else
                    return std::invoke(pfn, std::forward<Args>(args)...);
            },
            .data = reinterpret_cast<void *>(static_cast<result_type (*)(Args...)>(f))
        }
    {
    }

    template <typename F>
    explicit unique_function(F &&f) requires(std::is_invocable_v<F, Args...> &&std::is_constructible_v<std::remove_cvref_t<F>, F &&>)
        : unique_function(std::in_place_type<std::remove_cvref_t<F>>, std::forward<F>(f))
    {
    }

    template <typename T, typename... CArgs>
    unique_function(std::in_place_type_t<T>, CArgs &&...args) requires(std::is_invocable_v<T, Args...> &&std::is_constructible_v<T, CArgs...>)
    {
        void *const pointer = [&]() -> void * {
            // in order for the type to be stored inline it requires to be nothrow move constructible
            // so that unique_function's move operations are noexcept
            if constexpr (std::is_move_constructible_v<T> && std::is_nothrow_move_constructible_v<T>) {
                // check if the type can fit in the inline storage
                void *pointer = m_storage;
                size_t size = sizeof(m_storage);
                if (std::align(alignof(T), sizeof(T), pointer, size) != nullptr)
                    return pointer;
            }
            return nullptr;
        }();

        if (pointer) {
            std::construct_at(reinterpret_cast<std::remove_cvref_t<T> *>(pointer), std::forward<CArgs>(args)...);

            constinit static virtual_table const vtable {
                .move_construct = [](void *lhs, void *rhs) noexcept -> void {
                    auto *first = reinterpret_cast<std::remove_cvref_t<T> *>(lhs);
                    auto *second = reinterpret_cast<std::remove_cvref_t<T> *>(rhs);
                    std::construct_at(first, std::move(*second));
                },
                .destroy = [](void *data) noexcept -> void {
                    auto p = reinterpret_cast<std::remove_cvref_t<T> *>(data);
                    std::destroy_at(p);
                }
            };

            m_state = {
                .vtable = &vtable,
                .call = [](unique_function *self, Args &&...args) -> result_type {
                    auto &f = *reinterpret_cast<std::remove_cvref_t<T> *>(self->m_state.data);
                    if constexpr (std::is_void_v<result_type>)
                        (void)std::invoke(f, std::forward<Args>(args)...);
                    else
                        return std::invoke(f, std::forward<Args>(args)...);
                },
                .data = pointer
            };
        } else {
            constinit static virtual_table const vtable {
                .move_construct = +[](void *lhs, void *rhs) noexcept -> void {
                    auto *first = reinterpret_cast<std::remove_cvref_t<T> *>(lhs);
                    auto *second = reinterpret_cast<std::remove_cvref_t<T> *>(rhs);
                    std::construct_at(first, std::move(*second));
                },
                .destroy = +[](void *data) noexcept -> void {
                    auto *obj = reinterpret_cast<std::remove_cvref_t<T> *>(data);
                    delete obj;
                },
            };

            m_state = {
                .vtable = &vtable,
                .call = [](unique_function *self, Args &&...args) -> result_type {
                    auto &f = *reinterpret_cast<std::remove_cvref_t<T> *>(self->m_state.data);
                    if constexpr (std::is_void_v<result_type>)
                        (void)std::invoke(f, std::forward<Args>(args)...);
                    else
                        return std::invoke(f, std::forward<Args>(args)...);
                },
                .data = std::make_unique<std::remove_cvref_t<T>>(std::forward<CArgs>(args)...).release()
            };
        }
    }

    unique_function(unique_function const &) = delete;
    unique_function(unique_function &&other) noexcept
    {
        swap(*this, other);
    }

    unique_function &operator=(unique_function const &) = delete;
    unique_function &operator=(unique_function &&other) noexcept
    {
        swap(*this, other);
        return *this;
    }

    result_type operator()(Args... args)
    {
        return (*m_state.call)(this, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return m_state.call;
    }

    friend void swap(unique_function &first, unique_function &second) noexcept
    {
        using std::swap;
        bool const first_is_inline = first.m_state.data != nullptr && std::begin(first.m_storage) <= reinterpret_cast<char *>(first.m_state.data) && reinterpret_cast<char *>(first.m_state.data) < std::end(first.m_storage);
        bool const second_is_inline = second.m_state.data != nullptr && std::begin(second.m_storage) <= reinterpret_cast<char *>(second.m_state.data) && reinterpret_cast<char *>(second.m_state.data) < std::end(second.m_storage);
        if (first_is_inline && second_is_inline) {
            alignas(unique_function) char tmp_buf[sizeof(unique_function)];
            std::size_t const first_index = sizeof(virtual_state) + (reinterpret_cast<char *>(first.m_state.data) - std::begin(first.m_storage));
            std::size_t const second_index = sizeof(virtual_state) + (reinterpret_cast<char *>(second.m_state.data) - std::begin(second.m_storage));
            first.m_state.vtable->move_construct(tmp_buf + first_index, first.m_state.data);
            first.m_state.vtable->destroy(first.m_state.data);
            second.m_state.vtable->move_construct(first.m_storage + second_index - sizeof(virtual_state), second.m_state.data);
            second.m_state.vtable->destroy(second.m_state.data);
            first.m_state.vtable->move_construct(second.m_storage + first_index - sizeof(virtual_state), tmp_buf + first_index);
            first.m_state.vtable->destroy(tmp_buf + first_index);

            first.m_state.data = first.m_storage + second_index - sizeof(virtual_state);
            second.m_state.data = second.m_storage + first_index - sizeof(virtual_state);

            swap(first.m_state.vtable, second.m_state.vtable);
            swap(first.m_state.call, second.m_state.call);
        } else if (first_is_inline) {
            std::size_t const first_index = reinterpret_cast<char *>(first.m_state.data) - std::begin(first.m_storage);
            first.m_state.vtable->move_construct(second.m_storage + first_index, first.m_state.data);
            first.m_state.vtable->destroy(first.m_state.data);
            first.m_state.data = second.m_state.data;
            second.m_state.data = second.m_storage + first_index;
            swap(first.m_state.vtable, second.m_state.vtable);
            swap(first.m_state.call, second.m_state.call);
        } else if (second_is_inline) {
            swap(second, first);
        } else {
            swap(first.m_state.vtable, second.m_state.vtable);
            swap(first.m_state.call, second.m_state.call);
            swap(first.m_state.data, second.m_state.data);
        }
    }

    ~unique_function()
    {
        if (m_state.vtable && m_state.vtable->destroy && m_state.data)
            m_state.vtable->destroy(m_state.data);
        m_state.vtable = nullptr;
        m_state.call = nullptr;
        m_state.data = nullptr;
    }

private:
    virtual_state m_state;
    char m_storage[64 - sizeof(virtual_state)];
};

// sanity check
static_assert(alignof(unique_function<void()>) == 64, "unique_function alignment must be 64 bytes");
static_assert(sizeof(unique_function<void()>) == 64, "unique_function size must be 64 bytes");

}
#endif
