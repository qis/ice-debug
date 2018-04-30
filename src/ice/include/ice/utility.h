#pragma once
#include <ice/config.h>
#include <ice/handle.h>
#include <cstdint>

#ifndef ICE_LIKELY
#  ifdef __has_builtin
#    if __has_builtin(__builtin_expect)
#      define ICE_LIKELY(expr) __builtin_expect(expr, 1)
#    endif
#  endif
#endif
#ifndef ICE_LIKELY
#  define ICE_LIKELY(expr) expr
#endif

#ifndef ICE_UNLIKELY
#  ifdef __has_builtin
#    if __has_builtin(__builtin_expect)
#      define ICE_UNLIKELY(expr) __builtin_expect(expr, 0)
#    endif
#  endif
#endif
#ifndef ICE_UNLIKELY
#  define ICE_UNLIKELY(expr) expr
#endif

#ifdef __clang__
#  define ICE_OFFSETOF __builtin_offsetof
#else
#  define ICE_OFFSETOF(s, m) ((size_t)(&reinterpret_cast<char const volatile&>((((s*)0)->m))))
#endif

namespace ice {

class thread_local_storage {
public:
  struct close_type {
    void operator()(std::uintptr_t handle) noexcept;
  };

#if ICE_OS_WIN32
  using handle_type = ice::handle<std::uintptr_t, 0, close_type>;
#else
  using handle_type = ice::handle<std::uintptr_t, -1, close_type>;
#endif
  using handle_view = handle_type::view;

  thread_local_storage() noexcept;

  constexpr explicit operator bool() const noexcept {
    return handle_.valid();
  }

  void set(void* value) noexcept;

  void* get() noexcept;
  const void* get() const noexcept;

  handle_type& handle() noexcept {
    return handle_;
  }

  const handle_type& handle() const noexcept {
    return handle_;
  }

private:
  handle_type handle_;
};

}  // namespace ice
