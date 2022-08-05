#ifndef TCX_ASYNC_IORING_STAT_HPP
#define TCX_ASYNC_IORING_STAT_HPP

#include <functional>
#include <memory>

#include <tcx/async/concepts.hpp>
#include <tcx/async/wrap_op.hpp>
#include <tcx/native/handle.hpp>
#include <tcx/native/string.hpp>
#include <tcx/services/ioring_service.hpp>

namespace tcx {
namespace impl {

    struct ioring_statat_operation {
        using result_type = void;

        template <typename E, typename F>
        static void call(E &executor, tcx::ioring_service &service, int dir_fd, char const *pathname, struct ::stat *statbuf, int flags, F &&f)
        {
            using variant_type = std::variant<std::error_code, std::monostate>;

            auto statxbuf = std::make_unique<struct ::statx>();
            auto *const p = statxbuf.get();
            service.async_statx(dir_fd, pathname, flags, STATX_BASIC_STATS, p, [statxbuf = std::move(statxbuf), statbuf, &executor, f = std::forward<F>(f)](std::int32_t result) mutable {
                statbuf->st_dev = (static_cast<std::uint64_t>(statxbuf->stx_dev_major) << 32u) | statxbuf->stx_dev_minor;
                statbuf->st_ino = statxbuf->stx_ino;
                statbuf->st_nlink = statxbuf->stx_nlink;
                statbuf->st_mode = statxbuf->stx_mode;
                statbuf->st_uid = statxbuf->stx_uid;
                statbuf->st_gid = statxbuf->stx_gid;
                statbuf->st_rdev = (static_cast<std::uint64_t>(statxbuf->stx_rdev_major) << 32u) | statxbuf->stx_rdev_minor;
                statbuf->st_size = statxbuf->stx_size;
                statbuf->st_blksize = statxbuf->stx_blksize;
                statbuf->st_blocks = statxbuf->stx_blocks;
                statbuf->st_atim.tv_sec = statxbuf->stx_atime.tv_sec;
                statbuf->st_atim.tv_nsec = statxbuf->stx_atime.tv_nsec;
                statbuf->st_mtim.tv_sec = statxbuf->stx_mtime.tv_sec;
                statbuf->st_mtim.tv_nsec = statxbuf->stx_mtime.tv_nsec;
                statbuf->st_ctim.tv_sec = statxbuf->stx_ctime.tv_sec;
                statbuf->st_ctim.tv_nsec = statxbuf->stx_ctime.tv_nsec;
                statxbuf.reset();

                executor.post([f = std::move(f), result]() mutable {
                    if (result < 0)
                        std::invoke(f, variant_type(std::in_place_index<0>, result, std::system_category()));
                    else
                        std::invoke(f, variant_type(std::in_place_index<1>));
                });
            });
        }
    };

} // namespace impl

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_statat_operation::result_type>
auto async_stat(E &executor, tcx::ioring_service &service, tcx::native::c_string path, struct ::stat *statbuf, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_statat_operation>::call(executor, service, std::forward<F>(f), AT_FDCWD, path, statbuf, 0);
}

/**
 * @ingroup ioring_service
 */
template <typename E, typename F>
requires tcx::completion_handler<F, tcx::impl::ioring_statat_operation::result_type>
auto async_statat(E &executor, tcx::ioring_service &service, tcx::native::handle_type dir_fd, tcx::native::c_string path, struct ::stat *statbuf, int flags, F &&f)
{
    return tcx::impl::wrap_op<tcx::impl::ioring_statat_operation>::call(executor, service, std::forward<F>(f), dir_fd, path, statbuf, flags);
}

} // namespace tcx

#endif
