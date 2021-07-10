#ifndef TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP
#define TCX_UNSYNCHRONIZED_EXECUTION_CONTEXT_HPP

#include <deque>
#include <memory>
#include <memory_resource>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

#include <tcx/unique_function.hpp>

namespace tcx {

template <typename Allocator = std::pmr::polymorphic_allocator<std::byte>>
class unsynchronized_execution_context {
public:
    using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<std::byte>;

    unsynchronized_execution_context() noexcept
        : m_alloc {}
    {
    }

    unsynchronized_execution_context(allocator_type const &alloc) noexcept
        : m_alloc { alloc }
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
        while (!m_function_queue.empty()) {
            auto f = std::move(m_function_queue.front());
            m_function_queue.pop();
            f();
        }
        return m_function_queue.size();
    }

    template <typename T>
    T &use_service() requires(std::is_constructible_v<T, unsynchronized_execution_context &>)
    {
        if (auto it = m_services.find(typeid(T)); it == m_services.end()) {
            return make_service<T>();
        } else {
            return *reinterpret_cast<T *>(it->second.ptr);
        }
    }

    template <typename T>
    bool has_service() const noexcept
    {
        return m_services.find(typeid(T)) != m_services.end();
    }

    template <typename T>
    T &make_service()
    {
        if (has_service<T>())
            throw std::runtime_error("service already provided");
        auto *ptr = m_alloc.template new_object<T>(*this);
        auto dest = +[](allocator_type &alloc, void *ptr) {
            auto *service = reinterpret_cast<T *>(ptr);
            alloc.delete_object(service);
        };
        m_services.emplace(typeid(T), service_data { static_cast<void *>(ptr), dest });
        return *ptr;
    }

    ~unsynchronized_execution_context()
    {
        for (auto &[k, service] : m_services) {
            service.dest(m_alloc, service.ptr);
            service.ptr = nullptr;
            service.dest = nullptr;
        }
    }

private:
    allocator_type m_alloc;
    using function_storage = tcx::unique_function<void()>;
    std::queue<function_storage> m_function_queue;
    struct service_data {
        void *ptr;
        void (*dest)(allocator_type &alloc, void *);
    };
    std::unordered_map<std::type_index, service_data> m_services;
};
}
#endif
