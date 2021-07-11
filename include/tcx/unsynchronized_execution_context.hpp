#ifndef TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <deque>
#include <memory>
#include <memory_resource>
#include <queue>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include <tcx/is_service.hpp>
#include <tcx/unique_function.hpp>
#include <tcx/unique_types.hpp>

namespace tcx {

template <typename A, typename... S>
class unsynchronized_execution_context;

template <typename Allocator = std::pmr::polymorphic_allocator<std::byte>, typename... StaticServices>
class unsynchronized_execution_context {
public:
    static_assert(std::conjunction_v<tcx::is_service<std::remove_cvref_t<StaticServices>, unsynchronized_execution_context<Allocator>>...>, "A service doesn't implements the tcx::service concept");

    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;
    using static_services = tcx::unique_types_t<std::remove_cvref_t<StaticServices>...>;

    unsynchronized_execution_context()
        : unsynchronized_execution_context(allocator_type {})
    {
    }

    explicit unsynchronized_execution_context(allocator_type const &alloc)
        : m_alloc { alloc }
        , m_static_services(construct_static_services(*this))
    {
    }

    unsynchronized_execution_context(unsynchronized_execution_context const &) = delete;
    unsynchronized_execution_context(unsynchronized_execution_context &&) noexcept = default;

    unsynchronized_execution_context &operator=(unsynchronized_execution_context const &) = delete;
    unsynchronized_execution_context &operator=(unsynchronized_execution_context &&other) noexcept
    {
        if (this != &other) {
            std::destroy_at(&m_alloc);
            std::construct_at(&m_alloc, other.m_alloc);
            m_function_queue = std::move(other.m_function_queue);
            m_services = std::move(other.m_services);
        }
        return *this;
    }

    allocator_type get_allocator() const
    {
        return m_alloc;
    }

    template <typename F>
    void post(F &&functor)
    {
        m_function_queue.emplace(std::forward<F>(functor));
    }

    std::size_t run()
    {
        for (;;) {
            while (!m_function_queue.empty()) {
                auto f = std::move(m_function_queue.front());
                m_function_queue.pop();
                f();
            }
            poll_static_services(std::make_index_sequence<std::tuple_size_v<static_services>> {});
            poll_dynamic_servcies();
            if (m_function_queue.empty())
                break;
        }
        return m_function_queue.size();
    }

private:
    template <std::size_t I>
    void poll_static_service()
    {
        if constexpr (tcx::impl::has_conditional_poll<std::tuple_element_t<I, static_services>, unsynchronized_execution_context>) {
            std::get<I>(m_static_services).poll(*this, !m_function_queue.empty());
        } else if constexpr (tcx::impl::has_poll<std::tuple_element_t<I, static_services>, unsynchronized_execution_context>) {
            std::get<I>(m_static_services).poll(*this);
        }
    }

    template <std::size_t... I>
    void poll_static_services(std::index_sequence<I...>)
    {
        (poll_static_service<I>(), ...);
    }

    void poll_dynamic_servcies()
    {
        for (auto &[service_type, service_data] : m_services) {
            service_data.poll(service_data.ptr, *this, !m_function_queue.empty());
        }
    }

    template <typename Tuple, typename S>
    struct has_static_service;
    template <typename... Ts, typename S>
    struct has_static_service<std::tuple<Ts...>, S> : std::disjunction<std::is_same<Ts, S>...> {
    };

public:
    template <typename T>
    T &use_service() requires(std::is_constructible_v<std::remove_cvref_t<T>, unsynchronized_execution_context &>)
    {
        if constexpr (has_static_service<static_services, std::remove_cvref_t<T>>::value) {
            return std::get<std::remove_cvref_t<T>>(m_static_services);
        } else {
            if (auto it = m_services.find(typeid(T)); it == m_services.end()) {
                return make_service<T>();
            } else {
                return *reinterpret_cast<T *>(it->second.ptr);
            }
        }
    }

    template <typename T>
    bool has_service() const noexcept
    {
        if constexpr (has_static_service<static_services, std::remove_cvref_t<T>>::value) {
            return true;
        } else {
            return m_services.find(typeid(std::remove_cvref_t<T>)) != m_services.end();
        }
    }

    template <typename T>
    T &make_service()
    {
        if (has_service<T>())
            throw std::runtime_error("service already provided");
        auto *ptr = m_alloc.template new_object<std::remove_cvref_t<T>>(*this);
        auto dest = +[](allocator_type &alloc, void *ptr) {
            auto *service = reinterpret_cast<std::remove_cvref_t<T> *>(ptr);
            alloc.delete_object(service);
        };
        auto poll = +[](void *ptr, unsynchronized_execution_context &executor, bool should_block) {
            auto *p_service = reinterpret_cast<std::remove_cvref_t<T> *>(ptr);
            if constexpr (tcx::impl::has_conditional_poll<std::remove_cvref_t<T>, unsynchronized_execution_context>) {
                p_service->poll(executor, !executor.m_function_queue.empty());
            } else if constexpr (tcx::impl::has_poll<std::remove_cvref_t<T>, unsynchronized_execution_context>) {
                p_service->poll(executor);
            }
        };
        m_services.emplace(typeid(std::remove_cvref_t<T>), service_data { static_cast<void *>(ptr), poll, dest });
        return *ptr;
    }

    ~unsynchronized_execution_context()
    {
        for (auto &[k, service] : m_services) {
            service.dest(m_alloc, service.ptr);
            service.ptr = nullptr;
            service.poll = nullptr;
            service.dest = nullptr;
        }
    }

private:
    static static_services construct_static_services(unsynchronized_execution_context &self)
    {
        return construct_static_services(self, std::make_index_sequence<std::tuple_size_v<static_services>> {});
    }

    template <std::size_t... I>
    static static_services construct_static_services(unsynchronized_execution_context &self, std::index_sequence<I...>)
    {
        return static_services(std::tuple_element_t<I, static_services>(self)...);
    }

private:
    [[no_unique_address]] allocator_type m_alloc;
    [[no_unique_address]] static_services m_static_services;
    using function_storage = tcx::unique_function<void()>;
    std::queue<function_storage> m_function_queue;
    struct service_data {
        void *ptr;
        void (*poll)(void *, unsynchronized_execution_context &, bool);
        void (*dest)(allocator_type &alloc, void *);
    };
    std::unordered_map<std::type_index, service_data> m_services;
};
}
#endif
