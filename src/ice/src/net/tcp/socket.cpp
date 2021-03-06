#include <ice/net/tcp/socket.h>
#include <cassert>

#if ICE_OS_WIN32
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <mswsock.h>
#  include <new>
#else
#  include <sys/types.h>
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <unistd.h>
#endif

namespace ice::net::tcp {
namespace detail {

#if ICE_OS_WIN32

struct connect_ex {
  connect_ex() {
    net::socket::handle_type socket(::WSASocket(AF_INET, SOCK_STREAM, 0, nullptr, 0, WSA_FLAG_OVERLAPPED));
    if (!socket) {
      ec = ::WSAGetLastError();
      return;
    }
    auto fp = SIO_GET_EXTENSION_FUNCTION_POINTER;
    auto guid = GUID(WSAID_CONNECTEX);
    auto size = DWORD(sizeof(function));
    if (::WSAIoctl(socket, fp, &guid, sizeof(guid), &function, size, &size, nullptr, nullptr) == SOCKET_ERROR) {
      ec = ::WSAGetLastError();
      return;
    }
  }

  template <typename... Args>
  auto operator()(Args&&... args) const noexcept {
    assert(function);
    return function(std::forward<Args>(args)...);
  }

  ice::error_code ec;
  LPFN_CONNECTEX function = nullptr;
};

#endif

}  // namespace detail

socket::socket(ice::context& context, int family) : net::socket(context, family, SOCK_STREAM, IPPROTO_TCP) {
}

socket::socket(ice::context& context, int family, int protocol) : net::socket(context, family, SOCK_STREAM, protocol) {
}

void socket::listen(std::size_t backlog) {
  const auto size = backlog > 0 ? static_cast<int>(backlog) : SOMAXCONN;
#if ICE_OS_WIN32
  if (::listen(handle_, size) == SOCKET_ERROR) {
    throw ice::system_error(::WSAGetLastError(), "listen on socket");
  }
#else
  if (::listen(handle_, size) < 0) {
    throw ice::system_error(errno, "listen on socket");
  }
#endif
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  ::linger data = { 1, 0 };
  ::setsockopt(handle_, SOL_SOCKET, SO_LINGER, &data, sizeof(data));
#endif
}

bool accept::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  client_.endpoint().size() = client_.endpoint().capacity();
  auto& sockaddr = client_.endpoint().sockaddr();
  auto& size = client_.endpoint().size();
  client_.handle().reset(::accept4(socket_, &sockaddr, &size, SOCK_NONBLOCK));
  if (client_) {
    return true;
  }
  if (errno != EAGAIN && errno != EINTR) {
    ec_ = errno;
    return true;
  }
#endif
  return false;
}

bool accept::suspend() noexcept {
#if ICE_OS_WIN32
  const auto socket = socket_.as<SOCKET>();
  const auto client = client_.handle().as<SOCKET>();
  while (true) {
    if (::AcceptEx(socket, client, &buffer_, 0, buffer_size, buffer_size, &bytes_, get())) {
      break;
    }
    const auto rc = ::WSAGetLastError();
    if (rc == ERROR_IO_PENDING) {
      return true;
    }
    if (rc != WSAECONNRESET) {
      ec_ = rc;
      return false;
    }
  }
  return false;
#else
  return queue_recv(context_, socket_);
#endif
}

bool accept::resume() noexcept {
#if ICE_OS_WIN32
  DWORD bytes = 0;
  if (!::GetOverlappedResult(socket_.as<HANDLE>(), get(), &bytes, FALSE)) {
    if (const auto rc = ::GetLastError(); rc != WSAECONNRESET) {
      ec_ = rc;
      return true;
    }
    return false;
  }
  const auto addr = reinterpret_cast<::sockaddr_storage*>(&buffer_[buffer_size]);
  switch (addr->ss_family) {
  case AF_INET:
    client_.endpoint().sockaddr_in() = *reinterpret_cast<::sockaddr_in*>(addr);
    client_.endpoint().size() = sizeof(::sockaddr_in);
    break;
  case AF_INET6:
    client_.endpoint().sockaddr_in6() = *reinterpret_cast<::sockaddr_in6*>(addr);
    client_.endpoint().size() = sizeof(::sockaddr_in6);
    break;
  }
  return true;
#else
  return await_ready();
#endif
}

connect::connect(tcp::socket& socket, const net::endpoint& endpoint) noexcept :
  context_(socket.context().handle()), socket_(socket.handle()), endpoint_(endpoint) {
#if ICE_OS_WIN32
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = 0;
  if (::bind(socket_, reinterpret_cast<const SOCKADDR*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
    ec_ = ::WSAGetLastError();
  }
#endif
  socket.endpoint() = endpoint;
}

bool connect::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  while (true) {
    if (::connect(socket_, &endpoint_.sockaddr(), endpoint_.size()) == 0) {
      return true;
    }
    if (errno == EINPROGRESS) {
      break;
    }
    if (errno == EINTR) {
#  if ICE_OS_LINUX
      continue;
#  else
      break;
#  endif
    }
    ec_ = errno;
    break;
  }
#endif
  return false;
}

bool connect::suspend() noexcept {
#if ICE_OS_WIN32
  static const detail::connect_ex connect;
  if (connect.ec) {
    ec_ = connect.ec;
    return false;
  }
  const auto socket = socket_.as<SOCKET>();
  if (connect(socket, &endpoint_.sockaddr(), endpoint_.size(), nullptr, 0, nullptr, get())) {
    return false;
  }
  if (const auto rc = ::WSAGetLastError(); rc != ERROR_IO_PENDING) {
    ec_ = rc;
    return false;
  }
  return true;
#else
  return queue_send(context_, socket_);
#endif
}

bool connect::resume() noexcept {
#if ICE_OS_WIN32
  DWORD bytes = 0;
  const auto socket = socket_.as<HANDLE>();
  if (!::GetOverlappedResult(socket, get(), &bytes, FALSE)) {
    ec_ = ::GetLastError();
  }
#else
  auto code = 0;
  auto size = static_cast<socklen_t>(sizeof(code));
  if (::getsockopt(socket_, SOL_SOCKET, SO_ERROR, &code, &size) < 0) {
    ec_ = errno;
  } else if (code) {
    ec_ = code;
  }
#endif
  return true;
}

bool recv::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  if (const auto rc = ::read(socket_, buffer_.data, buffer_.size); rc >= 0) {
    buffer_.size = static_cast<std::size_t>(rc);
    return true;
  }
  if (errno == ECONNRESET) {
    buffer_.size = 0;
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
  if (::WSARecv(socket, buffer, 1, &bytes_, &flags_, get(), nullptr) != SOCKET_ERROR) {
    buffer_.size = bytes_;
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
  buffer_.size = bytes_;
  return true;
#else
  return await_ready();
#endif
}

bool send::await_ready() noexcept {
#if ICE_OS_LINUX || ICE_OS_FREEBSD
  if (const auto rc = ::write(socket_, buffer_.data, buffer_.size); rc > 0) {
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
  while (buffer_.size > 0) {
    const auto socket = socket_.as<SOCKET>();
    const auto buffer = std::launder(reinterpret_cast<LPWSABUF>(&buffer_));
    if (::WSASend(socket, buffer, 1, &bytes_, 0, get(), nullptr) == SOCKET_ERROR) {
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
  if (const auto rc = ::write(socket_, buffer_.data, buffer_.size); rc > 0) {
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
  if (::WSASend(socket, buffer, 1, &bytes_, 0, get(), nullptr) == SOCKET_ERROR) {
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

}  // namespace ice::net::tcp
