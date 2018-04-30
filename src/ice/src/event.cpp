#include <ice/event.h>
#include <new>

#if ICE_OS_WIN32
#  include <windows.h>
#elif ICE_OS_LINUX
#  include <sys/epoll.h>
#elif ICE_OS_FREEBSD
#  include <sys/event.h>
#endif

namespace ice {

static_assert(native_event_size == sizeof(native_event));
static_assert(native_event_alignment == alignof(native_event));

event::event() noexcept {
  new (get()) native_event{};
}

event::~event() {
  get()->~native_event();
}

#if ICE_OS_LINUX

bool event::queue_recv(int context, int id) noexcept {
  const auto nev = get();
  native_context_ = context;
  native_id_ = id;
  nev->events = EPOLLIN | EPOLLONESHOT;
  nev->data.ptr = this;
  if (::epoll_ctl(context, EPOLL_CTL_ADD, id, nev) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

bool event::queue_send(int context, int id) noexcept {
  const auto nev = get();
  native_context_ = context;
  native_id_ = id;
  nev->events = EPOLLOUT | EPOLLONESHOT;
  nev->data.ptr = this;
  if (::epoll_ctl(context, EPOLL_CTL_ADD, id, nev) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

bool event::queue_note(int context, int id) noexcept {
  const auto nev = get();
  nev->events = EPOLLOUT | EPOLLONESHOT;
  nev->data.ptr = this;
  if (::epoll_ctl(context, EPOLL_CTL_MOD, id, nev) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

void event::remove() noexcept {
  if (native_context_ != -1 && native_id_ != -1) {
    ::epoll_ctl(native_context_, EPOLL_CTL_DEL, native_id_, get());
    native_context_ = -1;
    native_id_ = -1;
  }
}

#elif ICE_OS_FREEBSD

bool event::queue_recv(int context, int id) noexcept {
  const auto nev = get();
  EV_SET(nev, static_cast<uintptr_t>(id), EVFILT_READ, EV_ADD | EV_ONESHOT, 0, 0, this);
  if (::kevent(context, nev, 1, nullptr, 0, nullptr) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

bool event::queue_send(int context, int id) noexcept {
  const auto nev = get();
  EV_SET(nev, static_cast<uintptr_t>(id), EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, this);
  if (::kevent(context, nev, 1, nullptr, 0, nullptr) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

bool event::queue_note(int context, int id) noexcept {
  const auto nev = get();
  EV_SET(nev, static_cast<uintptr_t>(id), EVFILT_USER, EV_ADD | EV_ONESHOT, NOTE_TRIGGER, 0, this);
  if (::kevent(context, nev, 1, nullptr, 0, nullptr) < 0) {
    ec_ = errno;
    return false;
  }
  return true;
}

#endif

}  // namespace ice
