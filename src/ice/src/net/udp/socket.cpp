#include <ice/net/udp/socket.h>
#include <cassert>

#if ICE_OS_WIN32
#  include <windows.h>
#  include <winsock2.h>
#  include <new>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#endif

namespace ice::net::udp {

socket::socket(ice::context& context, int family) : net::socket(context, family, SOCK_DGRAM, IPPROTO_UDP) {
}

socket::socket(ice::context& context, int family, int protocol) : net::socket(context, family, SOCK_DGRAM, protocol) {
}

bool recv::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  auto& sockaddr = endpoint_.sockaddr();
  auto& size = endpoint_.size();
  size = endpoint_.capacity();
  if (const auto rc = ::recvfrom(socket_, buffer_.data, buffer_.size, 0, &sockaddr, &size); rc >= 0) {
    size_ += static_cast<std::size_t>(rc);
    return true;
  }
  if (errno == ECONNRESET) {
    size_ = 0;
    return true;
  }
  if (errno != EAGAIN && errno != EINTR) {
    ec_ = errno;
    return true;
  }
#endif
  return false;
}

bool recv::suspend() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<SOCKET>();
  const auto buffer = std::launder(reinterpret_cast<LPWSABUF>(&buffer_));
  auto& sockaddr = endpoint_.sockaddr();
  auto& size = endpoint_.size();
  if (::WSARecvFrom(socket, buffer, 1, &bytes_, &flags_, &sockaddr, &size, get(), nullptr) != SOCKET_ERROR) {
    size_ += bytes_;
    return false;
  }
  if (const auto rc = ::WSAGetLastError(); rc != ERROR_IO_PENDING) {
    ec_ = rc;
    return false;
  }
  return true;
#else
  return queue_recv(context_, socket_);
#endif
}

bool recv::resume() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<HANDLE>();
  if (!::GetOverlappedResult(socket, get(), &bytes_, FALSE)) {
    ec_ = ::GetLastError();
  }
  size_ += bytes_;
  return true;
#else
  return await_ready();
#endif
}

bool send::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  const auto& sockaddr = endpoint_.sockaddr();
  const auto size = endpoint_.size();
  if (const auto rc = ::sendto(socket_, buffer_.data, buffer_.size, 0, &sockaddr, size); rc > 0) {
    assert(buffer_.size >= static_cast<std::size_t>(rc));
    buffer_.data += static_cast<std::size_t>(rc);
    buffer_.size -= static_cast<std::size_t>(rc);
    size_ += static_cast<std::size_t>(rc);
    return buffer_.size == 0;
  } else if (rc == 0) {
    return true;
  }
  if (errno != EAGAIN && errno != EINTR) {
    ec_ = errno;
    return true;
  }
#endif
  return false;
}

bool send::suspend() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<SOCKET>();
  const auto buffer = std::launder(reinterpret_cast<LPWSABUF>(&buffer_));
  const auto& sockaddr = endpoint_.sockaddr();
  const auto size = endpoint_.size();
  while (buffer_.size > 0) {
    if (::WSASendTo(socket, buffer, 1, &bytes_, 0, &sockaddr, size, get(), nullptr) == SOCKET_ERROR) {
      if (const auto rc = ::WSAGetLastError(); rc != ERROR_IO_PENDING) {
        ec_ = rc;
        break;
      }
      return true;
    }
    buffer_.data += bytes_;
    buffer_.size -= bytes_;
    size_ += bytes_;
    if (bytes_ == 0 || buffer_.size == 0) {
      break;
    }
  }
  return false;
#else
  return queue_send(context_, socket_);
#endif
}

bool send::resume() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<HANDLE>();
  if (!::GetOverlappedResult(socket, get(), &bytes_, FALSE)) {
    ec_ = ::GetLastError();
  } else {
    buffer_.data += bytes_;
    buffer_.data -= bytes_;
    size_ += bytes_;
    if (bytes_ > 0 && buffer_.size > 0) {
      return false;
    }
  }
  return true;
#else
  return await_ready();
#endif
}

bool send_some::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  const auto& sockaddr = endpoint_.sockaddr();
  const auto size = endpoint_.size();
  if (const auto rc = ::sendto(socket_, buffer_.data, buffer_.size, 0, &sockaddr, size); rc > 0) {
    assert(buffer_.size >= static_cast<std::size_t>(rc));
    buffer_.data += static_cast<std::size_t>(rc);
    buffer_.size -= static_cast<std::size_t>(rc);
    size_ += static_cast<std::size_t>(rc);
    return true;
  } else if (rc == 0) {
    return true;
  }
  if (errno != EAGAIN && errno != EINTR) {
    ec_ = errno;
    return true;
  }
#endif
  return false;
}

bool send_some::suspend() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<SOCKET>();
  const auto buffer = std::launder(reinterpret_cast<LPWSABUF>(&buffer_));
  const auto& sockaddr = endpoint_.sockaddr();
  const auto size = endpoint_.size();
  if (::WSASendTo(socket, buffer, 1, &bytes_, 0, &sockaddr, size, get(), nullptr) == SOCKET_ERROR) {
    if (const auto rc = ::WSAGetLastError(); rc != ERROR_IO_PENDING) {
      ec_ = rc;
      return false;
    }
    return true;
  }
  buffer_.data += bytes_;
  buffer_.size -= bytes_;
  size_ += bytes_;
  return false;
#else
  return queue_send(context_, socket_);
#endif
}

bool send_some::resume() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<HANDLE>();
  if (!::GetOverlappedResult(socket, get(), &bytes_, FALSE)) {
    ec_ = ::GetLastError();
  } else {
    buffer_.data += bytes_;
    buffer_.data -= bytes_;
    size_ += bytes_;
  }
  return true;
#else
  return await_ready();
#endif
}

}  // namespace ice::net::udp
