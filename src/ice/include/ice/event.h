#pragma once
#include <ice/config.h>
#include <ice/error.h>
#include <experimental/coroutine>
#include <type_traits>

#if ICE_OS_WIN32
typedef struct _OVERLAPPED OVERLAPPED;
#elif ICE_OS_LINUX
struct epoll_event;
#elif ICE_OS_FREEBSD
struct kevent;
#endif

namespace ice {

#if ICE_OS_WIN32
using native_event = OVERLAPPED;
#elif ICE_OS_LINUX
using native_event = struct epoll_event;
#elif ICE_OS_FREEBSD
using native_event = struct kevent;
#endif

struct event_base {
  std::aligned_storage_t<native_event_size, native_event_alignment> storage;
};

class event : public event_base {
public:
  event() noexcept;

#ifdef __INTELLISENSE__
  // clang-format off
  event(event&& other) {}
  event& operator=(event&& other) { return *this; }
  event(const event& other) {}
  event& operator=(const event& other) { return *this; }
  // clang-format on
#else
  event(event&& other) = delete;
  event& operator=(event&& other) = delete;
  event(const event& other) = delete;
  event& operator=(const event& other) = delete;
#endif

  virtual ~event();

  bool await_suspend(std::experimental::coroutine_handle<> awaiter) noexcept {
    awaiter_ = awaiter;
    return suspend();
  }

  void await_resume() noexcept {
#if ICE_OS_LINUX
    remove();
#endif
    if (resume() || !suspend()) {
      awaiter_.resume();
    }
  }

  virtual bool suspend() noexcept = 0;
  virtual bool resume() noexcept = 0;

  constexpr ice::error_code ec() const noexcept {
    return ec_;
  }

protected:
  native_event* get() noexcept {
    return reinterpret_cast<native_event*>(static_cast<event_base*>(this));
  }

#if ICE_OS_LINUX || ICE_OS_FREEBSD
  bool queue_recv(int context, int id) noexcept;
  bool queue_send(int context, int id) noexcept;
  bool queue_note(int context, int id) noexcept;
#endif

  ice::error_code ec_;

private:
#if ICE_OS_LINUX
  void remove() noexcept;

  int native_context_ = -1;
  int native_id_ = -1;
#endif

  std::experimental::coroutine_handle<> awaiter_;
};

}  // namespace ice
