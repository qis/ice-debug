#include <ice/utility.h>
#include <cassert>

#if ICE_OS_WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#endif

namespace ice {

void thread_local_storage::close_type::operator()(std::uintptr_t handle) noexcept {
#if ICE_OS_WIN32
  [[maybe_unused]] const auto rc = ::TlsFree(static_cast<DWORD>(handle));
  assert(rc);
#else
  [[maybe_unused]] const auto rc = ::pthread_key_delete(static_cast<pthread_key_t>(handle));
  assert(rc == 0);
#endif
}

thread_local_storage::thread_local_storage() noexcept {
#if ICE_OS_WIN32
  handle_.reset(::TlsAlloc());
#else
  pthread_key_t handle{};
  if (::pthread_key_create(&handle, nullptr) == 0) {
    handle_.reset(handle);
  }
#endif
  assert(handle_);
}

void thread_local_storage::set(void* value) noexcept {
#if ICE_OS_WIN32
  [[maybe_unused]] const auto rc = ::TlsSetValue(handle_.as<DWORD>(), value);
  assert(rc);
#else
  [[maybe_unused]] const auto rc = ::pthread_setspecific(handle_.as<pthread_key_t>(), value);
  assert(rc == 0);
#endif
}

void* thread_local_storage::get() noexcept {
#if ICE_OS_WIN32
  return ::TlsGetValue(handle_.as<DWORD>());
#else
  return ::pthread_getspecific(handle_.as<pthread_key_t>());
#endif
}

const void* thread_local_storage::get() const noexcept {
#if ICE_OS_WIN32
  return ::TlsGetValue(handle_.as<DWORD>());
#else
  return ::pthread_getspecific(handle_.as<pthread_key_t>());
#endif
}

}  // namespace ice
