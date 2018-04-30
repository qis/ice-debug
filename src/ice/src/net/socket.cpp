#include <ice/net/socket.h>

#if ICE_OS_WIN32
#  include <windows.h>
#  include <winsock2.h>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace ice::net {

#if ICE_OS_WIN32

static_assert(INVALID_SOCKET == socket::handle_type::invalid_value());

void socket::close_type::operator()(std::uintptr_t handle) noexcept {
  ::closesocket(static_cast<SOCKET>(handle));
}

#else

void socket::close_type::operator()(int handle) noexcept {
  while (::close(handle) && errno == EINTR) {
  }
}

#endif

socket::socket(ice::context& context, int family, int type, int protocol) : context_(context), family_(family) {
#if ICE_OS_WIN32
  handle_.reset(::WSASocket(family, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED));
  if (!handle_) {
    throw ice::system_error(::WSAGetLastError(), "create socket");
  }
  if (!::CreateIoCompletionPort(handle_.as<HANDLE>(), context.handle().as<HANDLE>(), 0, 0)) {
    throw ice::system_error(::GetLastError(), "create socket completion port");
  }
  if (!::SetFileCompletionNotificationModes(handle_.as<HANDLE>(), FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
    throw ice::system_error(::GetLastError(), "set socket completion port modes");
  }
#else
  handle_.reset(::socket(family, type | SOCK_NONBLOCK, protocol));
  if (!handle_) {
    throw ice::system_error(errno, "create socket");
  }
#endif
}

void socket::bind(const net::endpoint& endpoint) {
#if ICE_OS_WIN32
  if (::bind(handle_, &endpoint.sockaddr(), endpoint.size()) == SOCKET_ERROR) {
    throw ice::system_error(::WSAGetLastError(), "bind socket");
  }
#else
  while (::bind(handle_, &endpoint.sockaddr(), endpoint.size()) < 0) {
    if (errno != EINTR) {
      throw ice::system_error(errno, "bind socket");
    }
  }
#endif
  endpoint_ = endpoint;
}

void socket::shutdown(net::shutdown direction) {
#if ICE_OS_WIN32
  auto value = 0;
  switch (direction) {
  case net::shutdown::recv: value = SD_RECEIVE; break;
  case net::shutdown::send: value = SD_SEND; break;
  case net::shutdown::both: value = SD_BOTH; break;
  }
  if (::shutdown(handle_, value) == SOCKET_ERROR) {
    throw ice::system_error(::WSAGetLastError(), "shutdown socket");
  }
#else
  auto value = 0;
  switch (direction) {
  case net::shutdown::recv: value = SHUT_RD; break;
  case net::shutdown::send: value = SHUT_WR; break;
  case net::shutdown::both: value = SHUT_RDWR; break;
  }
  if (::shutdown(handle_, value) < 0) {
    throw ice::system_error(errno, "shutdown socket");
  }
#endif
}

int socket::type() const noexcept {
  auto data = 0;
  auto size = socklen_t(sizeof(data));
  if (get(SOL_SOCKET, SO_TYPE, &data, size)) {
    return 0;
  }
  return data;
}

int socket::protocol() const noexcept {
#if ICE_OS_WIN32
  WSAPROTOCOL_INFO data;
  auto size = socklen_t(sizeof(data));
  if (get(SOL_SOCKET, SO_PROTOCOL_INFO, &data, size)) {
    return 0;
  }
  return data.iProtocol;
#else
  auto data = 0;
  auto size = socklen_t(sizeof(data));
  if (get(SOL_SOCKET, SO_PROTOCOL, &data, size)) {
    return 0;
  }
  return data;
#endif
}

ice::error_code socket::get(int level, int name, void* data, socklen_t& size) const noexcept {
#if ICE_OS_WIN32
  if (::getsockopt(handle_, level, name, reinterpret_cast<char*>(data), &size) == SOCKET_ERROR) {
    return ::WSAGetLastError();
  }
#else
  if (::getsockopt(handle_, level, name, data, &size) < 0) {
    return errno;
  }
#endif
  return {};
}

ice::error_code socket::set(int level, int name, const void* data, socklen_t size) noexcept {
#if ICE_OS_WIN32
  if (::setsockopt(handle_, level, name, reinterpret_cast<const char*>(data), size) == SOCKET_ERROR) {
    return ::WSAGetLastError();
  }
#else
  if (::setsockopt(handle_, level, name, data, size) < 0) {
    return errno;
  }
#endif
  return {};
}

}  // namespace ice::net
