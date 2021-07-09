#pragma once

#include <functional>
#include <type_traits>

namespace tcx {

template <typename T>
struct function_reference;

template <typename R>
struct function_reference<R()> {
    using result_type = R;

private:
    using function_pointer_type = result_type (*)(void*);

public:
    constexpr function_reference() noexcept = default;
    constexpr function_reference(std::nullptr_t) noexcept
        : function_reference()
    {
    }

    template <typename U>
    function_reference(U (*function_pointer)()) noexcept requires std::is_convertible_v<U, R>
        : m_data { reinterpret_cast<void*>(function_pointer) },
          m_func {
              +[](void* data) -> R {
                  auto* function_pointer = reinterpret_cast<R (*)()>(data);
                  return function_pointer();
              }
          }
    {
    }

    template <typename F>
    function_reference(F& f) noexcept requires std::is_invocable_r_v<R, F>
        : m_data { reinterpret_cast<void*>(&f) },
          m_func { +[](void* data) -> R {
              auto& function_object = *reinterpret_cast<F*>(data);
              return function_object();
          } }
    {
    }

    constexpr result_type operator()()
    {
        if (!m_func)
            throw std::bad_function_call {};
        return m_func(m_data);
    }

    constexpr explicit operator bool() const noexcept
    {
        return m_func;
    }

private:
    void* m_data = nullptr;
    function_pointer_type m_func = nullptr;
};

template <typename R>
struct function_reference<R() const> {
public:
    using result_type = R;

private:
    using function_pointer_type = result_type (*)(void*);

public:
    constexpr function_reference() noexcept = default;
    constexpr function_reference(std::nullptr_t) noexcept
        : function_reference()
    {
    }

    template <typename U>
    function_reference(U (*function_pointer)()) noexcept requires std::is_convertible_v<U, R>
        : m_data { reinterpret_cast<void*>(function_pointer) },
          m_func {
              +[](void* data) -> R {
                  auto* function_pointer = reinterpret_cast<R (*)()>(data);
                  return function_pointer();
              }
          }
    {
    }

    template <typename F>
    function_reference(F& f) noexcept requires std::is_invocable_r_v<R, F>
        : m_data { reinterpret_cast<void*>(&f) },
          m_func {
              +[](void* data) -> R {
                  auto& function_object = *reinterpret_cast<F*>(data);
                  return function_object();
              }
          }
    {
    }

    constexpr result_type operator()() const
    {
        if (!m_func)
            throw std::bad_function_call {};
        return m_func(m_data);
    }

    constexpr explicit operator bool() const noexcept
    {
        return m_func;
    }

private:
    void* m_data = nullptr;
    function_pointer_type m_func = nullptr;
};

} // namespace tcx
