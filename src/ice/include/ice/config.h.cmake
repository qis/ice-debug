#pragma once
#include <cstddef>

#if defined(WIN32)
#  define ICE_OS_WIN32 1
#elif defined(__linux__)
#  define ICE_OS_LINUX 1
#elif defined(__FreeBSD__)
#  define ICE_OS_FREEBSD 1
#else
#  error Unsupported OS
#endif

#ifndef ICE_OS_WIN32
#  define ICE_OS_WIN32 0
#endif

#ifndef ICE_OS_LINUX
#  define ICE_OS_LINUX 0
#endif

#ifndef ICE_OS_FREEBSD
#  define ICE_OS_FREEBSD 0
#endif

namespace ice {

constexpr inline std::size_t native_event_size = @native_event_size@;
constexpr inline std::size_t native_event_alignment = @native_event_alignment@;

constexpr inline std::size_t sockaddr_storage_size = @sockaddr_storage_size@;
constexpr inline std::size_t sockaddr_storage_alignment = @sockaddr_storage_alignment@;

constexpr inline std::size_t linger_size = @linger_size@;
constexpr inline std::size_t linger_alignment = @linger_alignment@;

}  // namespace ice
