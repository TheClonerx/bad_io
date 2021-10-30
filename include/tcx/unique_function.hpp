#ifndef TCX_UNIQUE_FUNCTION_HPP
#define TCX_UNIQUE_FUNCTION_HPP

#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace tcx {

namespace impl {
    template <typename T>
    struct is_reference_wrapper : std::bool_constant<false> {
    };

    template <typename T>
    struct is_reference_wrapper<std::reference_wrapper<T>> : std::bool_constant<true> {
    };
}

template <typename F>
class unique_function;

template <typename R, typename... Args>
class alignas(64) unique_function<R(Args...)> {
public:
    using result_type = R;
    using arguments_tuple = std::tuple<Args &&...>;

private:
    using func_call_t = result_type (*)(unique_function *self, Args &&...);
    using func_destroy_t = void (*)(void *data);
    using func_move_t = void (*)(void *lhs, void *rhs);

    struct virtual_table {
        func_move_t move_construct;
        func_destroy_t destroy;
    };

    struct virtual_state {
        virtual_table const *vtable = nullptr;
        func_call_t call = nullptr;
        void *data = nullptr;
    };

public:
    unique_function() noexcept = default;
    unique_function(std::nullptr_t) noexcept
        : unique_function()
    {
    }

    template <typename F>
    explicit unique_function(F &&f) noexcept(std::is_convertible_v<std::remove_cvref_t<F>, result_type (*)(Args...)>) requires(std::is_invocable_r_v<R, F, Args...>)
    {
        if constexpr (tcx::impl::is_reference_wrapper<std::remove_cvref_t<F>>::value) {
            m_state.data = reinterpret_cast<void *>(&f.get());
            m_state.call = +[](unique_function *self, Args &&...args) -> result_type {
                auto &ref = *reinterpret_cast<typename F::type *>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    (void)std::invoke(ref, std::forward<Args>(args)...);
                else
                    return std::invoke(ref, std::forward<Args>(args)...);
            };
        } else if constexpr (std::is_convertible_v<std::remove_cvref_t<F>, result_type (*)(Args...)>) { // a function pointer/reference or a stateless lambda was passed
            m_state.data = reinterpret_cast<void *>(static_cast<result_type (*)(Args...)>(f));
            m_state.call = +[](unique_function *self, Args &&...args) -> result_type {
                auto *f = reinterpret_cast<result_type (*)(Args...)>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    return static_cast<result_type>((*f)(std::forward<Args>(args)...));
                else
                    return (*f)(std::forward<Args>(args)...);
            };
        } else {
            new (this) unique_function(std::in_place_type<F>, std::forward<F>(f));
        }
    }

    template <typename T, typename... CArgs>
    requires std::invocable<T, Args...>
    unique_function(std::in_place_type_t<T>, CArgs &&...args)
    {
        bool store_inline = std::is_move_constructible_v<T> && std::is_nothrow_move_constructible_v<T>;
        void *pointer = m_storage;
        if (store_inline) {
            size_t size = sizeof(m_storage);
            if (std::align(alignof(T), sizeof(T), pointer, size) == nullptr)
                store_inline = false;
        }

        if (store_inline) {
            std::construct_at(reinterpret_cast<std::remove_cvref_t<T> *>(pointer), std::forward<CArgs>(args)...);
            func_call_t call = +[](unique_function *self, Args &&...args) -> result_type {
                auto &f = *reinterpret_cast<std::remove_cvref_t<T> *>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    (void)std::invoke(f, std::forward<Args>(args)...);
                else
                    return std::invoke(f, std::forward<Args>(args)...);
            };

            static auto const table = []() {
                virtual_table result {};
                result.destroy = +[](void *data) -> void {
                    auto p = reinterpret_cast<std::remove_cvref_t<T> *>(data);
                    std::destroy_at(p);
                };

                result.move_construct = +[](void *lhs, void *rhs) {
                    auto *second = reinterpret_cast<std::remove_cvref_t<T> *>(rhs);
                    new (lhs) std::remove_cvref_t<T>(std::move(*second));
                };
                return result;
            }();
            m_state.data = pointer;
            m_state.call = call;
            m_state.vtable = &table;
        } else {
            auto *data = new std::remove_cvref_t<T>(std::forward<CArgs>(args)...);

            func_call_t call = +[](unique_function *self, Args &&...args) -> result_type {
                auto &f = *reinterpret_cast<std::remove_cvref_t<T> *>(self->m_state.data);
                if constexpr (std::is_void_v<result_type>)
                    (void)std::invoke(f, std::forward<Args>(args)...);
                else
                    return std::invoke(f, std::forward<Args>(args)...);
            };

            static auto const table = []() {
                virtual_table result;
                result.destroy = +[](void *data) -> void {
                    auto *obj = reinterpret_cast<std::remove_cvref_t<T> *>(data);
                    delete obj;
                };

                result.move_construct = +[](void *lhs, void *rhs) {
                    auto *second = reinterpret_cast<std::remove_cvref_t<T> *>(rhs);
                    new (lhs) std::remove_cvref_t<T>(std::move(*second));
                };

                return result;
            }();
            m_state.data = reinterpret_cast<void *>(data);
            m_state.call = call;
            m_state.vtable = &table;
        }
    }

    unique_function(unique_function const &) = delete;
    unique_function(unique_function &&other) noexcept
    {
        swap(*this, other);
    }

    unique_function &operator=(unique_function const &) = delete;
    unique_function &operator=(unique_function &&other)
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
        bool const first_is_inline = std::begin(first.m_storage) < reinterpret_cast<char *>(first.m_state.data) && reinterpret_cast<char *>(first.m_state.data) < std::end(first.m_storage);
        bool const second_is_inline = std::begin(second.m_storage) < reinterpret_cast<char *>(second.m_state.data) && reinterpret_cast<char *>(second.m_state.data) < std::end(second.m_storage);
        if (first_is_inline && second_is_inline) {
            alignas(64) char tmp_buf[64];
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

}
#endif
