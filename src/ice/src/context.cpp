#include <ice/context.h>
#include <vector>

#if ICE_OS_WIN32
#  include <windows.h>
#  include <winsock2.h>
#elif ICE_OS_LINUX
#  include <sys/epoll.h>
#  include <sys/eventfd.h>
#  include <unistd.h>
#elif ICE_OS_FREEBSD
#  include <sys/event.h>
#  include <unistd.h>
#endif

namespace ice {
namespace detail {

#if ICE_OS_WIN32

struct wsa {
  wsa() {
    WSADATA wsadata = {};
    if (const auto rc = ::WSAStartup(MAKEWORD(2, 2), &wsadata)) {
      throw ice::system_error(rc, "wsa startup");
    }
    const auto major = LOBYTE(wsadata.wVersion);
    const auto minor = HIBYTE(wsadata.wVersion);
    if (major < 2 || (major == 2 && minor < 2)) {
      throw ice::system_error(ice::errc::version, "wsa startup");
    }
  }
};

#endif

}  // namespace detail

#if ICE_OS_WIN32

void context::close_type::operator()(std::uintptr_t handle) noexcept {
  ::CloseHandle(reinterpret_cast<HANDLE>(handle));
}

#else

void context::close_type::operator()(int handle) noexcept {
  while (::close(handle) && errno == EINTR) {
  }
}

#endif

context::context() {
#if ICE_OS_WIN32
  static const detail::wsa wsa;
  handle_type handle(::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0));
  if (!handle) {
    throw ice::system_error(::GetLastError(), "create context");
  }
#elif ICE_OS_LINUX
  handle_type handle(::epoll_create1(0));
  if (!handle) {
    throw ice::system_error(errno, "create context");
  }
  handle_type events(::eventfd(0, EFD_NONBLOCK));
  if (!events) {
    throw ice::system_error(errno, "create context event");
  }
  static epoll_event nev = { EPOLLONESHOT, {} };
  if (::epoll_ctl(handle, EPOLL_CTL_ADD, events, &nev) < 0) {
    throw ice::system_error(errno, "add context event");
  }
  events_ = std::move(events);
#elif ICE_OS_FREEBSD
  handle_type handle(::kqueue());
  if (!handle) {
    throw ice::system_error(errno, "create context");
  }
  struct kevent nev = {};
  EV_SET(&nev, 0, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, nullptr);
  if (::kevent(handle, &nev, 1, nullptr, 0, nullptr) < 0) {
    throw ice::system_error(errno, "create context event");
  }
#endif
  handle_ = std::move(handle);
}

context::~context() {
  [[maybe_unused]] const auto state = state_.fetch_or(stop_requested_flag, std::memory_order_release);
  [[maybe_unused]] const auto thread_count = state / thread_count_increment;
  assert(thread_count == 0);
}

void context::run(std::size_t event_buffer_size) {
#if ICE_OS_WIN32
  using data_type = OVERLAPPED_ENTRY;
  using size_type = ULONG;
#else
  using data_type = ice::native_event;
  using size_type = int;
#endif

  ice::error_code ec;
  std::vector<data_type> events;
  events.resize(event_buffer_size);

  const auto events_data = events.data();
  const auto events_size = static_cast<size_type>(events.size());

  index_.set(this);
  state_.fetch_add(thread_count_increment, std::memory_order_relaxed);
  while (true) {
#if ICE_OS_WIN32
    size_type count = 0;
    if (!::GetQueuedCompletionStatusEx(handle_.as<HANDLE>(), events_data, events_size, &count, INFINITE, FALSE)) {
      if (const auto rc = ::GetLastError(); rc != ERROR_ABANDONED_WAIT_0) {
        ec = rc;
      }
      break;
    }
#elif ICE_OS_LINUX
    const auto count = ::epoll_wait(handle_, events_data, events_size, -1);
    if (count < 0 && errno != EINTR) {
      ec = errno;
      break;
    }
#elif ICE_OS_FREEBSD
    const auto count = ::kevent(handle_, nullptr, 0, events_data, events_size, nullptr);
    if (count < 0 && errno != EINTR) {
      ec = errno;
      break;
    }
#endif
    bool interrupted = false;
    for (size_type i = 0; i < count; i++) {
      auto& entry = events_data[i];
#if ICE_OS_WIN32
      if (const auto ev = static_cast<ice::event*>(reinterpret_cast<ice::event_base*>(entry.lpOverlapped))) {
        ev->await_resume();
        continue;
      }
#elif ICE_OS_LINUX
      if (const auto ev = reinterpret_cast<ice::event*>(entry.data.ptr)) {
        ev->await_resume();
        continue;
      }
#elif ICE_OS_FREEBSD
      if (const auto ev = reinterpret_cast<ice::event*>(entry.udata)) {
        ev->await_resume();
        continue;
      }
#endif
      interrupted = true;
    }
    if (interrupted) {
      if (state_.load(std::memory_order_acquire) & stop_requested_flag) {
        break;
      }
    }
  }
  state_.fetch_sub(thread_count_increment, std::memory_order_release);
  index_.set(nullptr);
  interrupt();
  if (ec) {
    throw ice::system_error(ec, "context");
  }
}

void context::interrupt() noexcept {
#if ICE_OS_WIN32
  ::PostQueuedCompletionStatus(handle_.as<HANDLE>(), 0, 0, nullptr);
#elif ICE_OS_LINUX
  static epoll_event nev{ EPOLLOUT | EPOLLONESHOT, {} };
  ::epoll_ctl(handle_, EPOLL_CTL_MOD, events_, &nev);
#elif ICE_OS_FREEBSD
  struct kevent nev {};
  EV_SET(&nev, 0, EVFILT_USER, 0, NOTE_TRIGGER, 0, nullptr);
  ::kevent(handle_, &nev, 1, nullptr, 0, nullptr);
#endif
}

bool context::stop() noexcept {
  const auto state = state_.fetch_or(stop_requested_flag, std::memory_order_release);
  const auto thread_count = state / thread_count_increment;
  interrupt();
  return thread_count == 0;
}

bool schedule::suspend() noexcept {
#if ICE_OS_WIN32
  if (!::PostQueuedCompletionStatus(context_.as<HANDLE>(), 0, 0, get())) {
    ec_ = ::GetLastError();
    return false;
  }
  return true;
#elif ICE_OS_LINUX
  return queue_note(context_, handle_);
#elif ICE_OS_FREEBSD
  return queue_note(context_, 0);
#endif
}

}  // namespace ice
