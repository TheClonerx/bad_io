#ifndef TCX_NATIVE_RESULT_HPP
#define TCX_NATIVE_RESULT_HPP

#include "tcx/utilities/nonbool_integer.hpp"
#include <cassert>
#include <cstdint>
#include <deque>
#include <memory>
#include <system_error>
#include <type_traits>
#include <utility>
namespace tcx::native {

#ifdef __unix__
using error_type = int;
#else
using error_type = unsigned;
#endif

template <typename T>
struct maybe_uninitialized {
    union type {
        T value;

        type()
        {
        }

        ~type()
        {
        }
    };
};

template <typename T>
using maybe_uninitialized_t = typename maybe_uninitialized<T>::type;

template <typename T>
requires(tcx::utilities::is_nonbool_integral_v<T> || std::is_pointer_v<T>)
struct syscall;

template <typename T>

struct result {
    using error_type = tcx::native::error_type;
    using value_type = T;
    struct storage_type {
        error_type first;
        std::conditional_t<std::is_reference_v<value_type>, std::add_pointer_t<value_type>, maybe_uninitialized_t<value_type>> second;
    };

    static std::error_category const &category() noexcept
    {
        return std::generic_category();
    }

    [[nodiscard]] constexpr value_type const &value() const &noexcept

    {
        assert(has_value());
        if constexpr (std::is_reference_v<value_type>) {
            return *m_value.second;
        } else {
            return m_value.second.value;
        }
    }

    [[nodiscard]] constexpr value_type &value() &noexcept

    {
        assert(has_value());
        if constexpr (std::is_reference_v<value_type>) {
            return *m_value.second;
        } else {
            return m_value.second.value;
        }
    }

    [[nodiscard]] constexpr value_type &&value() &&noexcept
    {
        assert(has_value());
        if constexpr (std::is_reference_v<value_type>) {
            return static_cast<value_type &&>(*m_value.second);
        } else {
            return static_cast<value_type &&>(m_value.second.value);
        }
    }

    [[nodiscard]] constexpr value_type const &&value() const &&noexcept
    {
        assert(has_value());
        if constexpr (std::is_reference_v<value_type>) {
            return static_cast<value_type &&>(*m_value.second);
        } else {
            return static_cast<value_type &&>(m_value.second.value);
        }
    }

    [[nodiscard]] constexpr error_type error() const noexcept
    {
        return m_value.first;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_value.first;
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !m_value.first;
    }

    constexpr static result from_error(error_type error) noexcept
    {
        result ret;
        ret.m_value.first = error;
        if constexpr (std::is_default_constructible_v<value_type>) {
            if (error == error_type {}) {
                std::construct_at(std::addressof(ret.m_value.second));
            }
        } else {
            assert(error != error_type {});
        }

        return ret;
    }

    constexpr static result from_value(value_type &&value) noexcept
    {
        result ret;
        ret.m_value.first = error_type {};
        std::construct_at(std::addressof(ret.m_value.second.value), std::move(value));
        return ret;
    }

    constexpr static result from_value(value_type const &value) noexcept
    {
        result ret;
        ret.m_value.first = error_type {};
        std::construct_at(std::addressof(ret.m_value.second.value), value);
        return ret;
    }

    constexpr result() noexcept(std::is_nothrow_default_constructible_v<error_type>) = default;

    [[nodiscard]] constexpr result(result &&other) noexcept(std::is_nothrow_move_constructible_v<error_type> &&std::is_nothrow_move_constructible_v<value_type>)
    requires(std::is_move_constructible_v<error_type> && std::is_move_constructible_v<value_type>)
    {
        if (other.has_value()) {
            std::construct_at(std::addressof(m_value.second.value), std::move(other.value()));
        }
        m_value.first = other.m_value.first;
    }

    [[nodiscard]] constexpr result(result const &other) noexcept(std::is_nothrow_copy_constructible_v<error_type> &&std::is_nothrow_copy_constructible_v<value_type>)
    requires(std::is_copy_constructible_v<error_type> && std::is_copy_constructible_v<value_type>)
    {
        if (other.has_value()) {
            std::construct_at(std::addressof(m_value.second), other.value());
        }
        m_value.first = other.m_value.first;
    }

    ~result()
    requires(!std::is_trivially_destructible_v<value_type>)
    {
        if (has_value())
            std::destroy_at(std::addressof(m_value.second));
    }

    ~result()
    requires(std::is_trivially_destructible_v<value_type>)
    = default;

private:
    storage_type m_value;
};

template <typename T>
requires std::is_void_v<T> || std::is_empty_v<T>
struct result<T> {
    using error_type = tcx::native::error_type;
    using value_type = void;
    using storage_type = error_type;

    static std::error_category const &category() noexcept
    {
        return std::generic_category();
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return m_value;
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !has_value();
    }

    constexpr void value() const noexcept
    {
        assert(has_value());
    }

    [[nodiscard]] constexpr error_type error() const noexcept
    {
        return m_value;
    }

    constexpr static result from_error(error_type error) noexcept
    {
        result ret;
        ret.m_value = error;
        return ret;
    }

    constexpr static result from_value() noexcept
    {
        result ret;
        return ret;
    }

private:
    storage_type m_value {};
};

template <typename T>
struct result<syscall<T>> {
    using error_type = tcx::native::error_type;
    using value_type = T;
    using storage_type = std::conditional_t<std::is_pointer_v<value_type>, std::uintptr_t, std::common_type_t<error_type, value_type>>;

    static std::error_category const &category() noexcept
    {
        return std::system_category();
    }

    constexpr value_type value() const noexcept
    {
        assert(has_value());
        if constexpr (std::is_pointer_v<value_type>) {
            return reinterpret_cast<value_type>(m_value);
        } else {
            return static_cast<value_type>(m_value);
        }
    }

    [[nodiscard]] constexpr error_type error() const noexcept
    {
        if (has_error())
            return static_cast<error_type>(m_value);
        else
            return error_type {};
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        constexpr auto max_error = static_cast<storage_type>(-1);
        constexpr auto min_error = static_cast<storage_type>(-1024 * 4);

        return min_error <= m_value && m_value <= max_error;
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !has_value();
    }

private:
    storage_type m_value;
};

} // namespace tcx::native

#endif
