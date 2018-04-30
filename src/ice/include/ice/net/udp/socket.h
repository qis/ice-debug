#pragma once
#include <ice/config.h>
#include <ice/context.h>
#include <ice/error.h>
#include <ice/event.h>
#include <ice/net/buffer.h>
#include <ice/net/socket.h>
#include <cstddef>

// WARNING: Work in progress. Do not use!
// WARNING: Error codes EMSGSIZE, WSAENOBUFS, ERROR_MORE_DATA and ERROR_INVALID_USER_BUFFER are not handled.

namespace ice::net::udp {

class recv;
class send;
class send_some;

class socket : public net::socket {
public:
  explicit socket(ice::context& context) noexcept : net::socket(context) {
  }

  socket(ice::context& context, int family);
  socket(ice::context& context, int family, int protocol);

  udp::recv recv(net::endpoint& endpoint, char* data, std::size_t size);
  udp::send send(const net::endpoint& endpoint, const char* data, std::size_t size);
  udp::send_some send_some(const net::endpoint& endpoint, const char* data, std::size_t size);
};

class recv final : public ice::event {
public:
  recv(udp::socket& socket, net::endpoint& endpoint, char* data, std::size_t size) noexcept :
    context_(socket.context().handle()), socket_(socket.handle()), endpoint_(endpoint), buffer_(data, size) {
  }

  bool await_ready() noexcept;
  bool suspend() noexcept override;
  bool resume() noexcept override;

  std::size_t await_resume() {
    if (ec_) {
      throw ice::system_error(ec_, "udp recv");
    }
    return size_;
  }

private:
  ice::context::handle_view context_;
  net::socket::handle_view socket_;
  net::endpoint& endpoint_;
  net::buffer buffer_;
  std::size_t size_ = 0;
#if ICE_OS_WIN32
  unsigned long bytes_ = 0;
  unsigned long flags_ = 0;
#endif
};

class send final : public ice::event {
public:
  send(udp::socket& socket, const net::endpoint& endpoint, const char* data, std::size_t size) noexcept :
    context_(socket.context().handle()), socket_(socket.handle()), endpoint_(endpoint), buffer_(data, size) {
  }

  bool await_ready() noexcept;
  bool suspend() noexcept override;
  bool resume() noexcept override;

  std::size_t await_resume() {
    if (ec_) {
      throw ice::system_error(ec_, "udp send");
    }
    return size_;
  }

private:
  ice::context::handle_view context_;
  net::socket::handle_view socket_;
  const net::endpoint& endpoint_;
  net::const_buffer buffer_;
  std::size_t size_ = 0;
#if ICE_OS_WIN32
  unsigned long bytes_ = 0;
#endif
};

class send_some final : public ice::event {
public:
  send_some(udp::socket& socket, const net::endpoint& endpoint, const char* data, std::size_t size) noexcept :
    context_(socket.context().handle()), socket_(socket.handle()), endpoint_(endpoint), buffer_(data, size) {
  }

  bool await_ready() noexcept;
  bool suspend() noexcept override;
  bool resume() noexcept override;

  std::size_t await_resume() {
    if (ec_) {
      throw ice::system_error(ec_, "udp send some");
    }
    return size_;
  }

private:
  ice::context::handle_view context_;
  net::socket::handle_view socket_;
  const net::endpoint& endpoint_;
  net::const_buffer buffer_;
  std::size_t size_ = 0;
#if ICE_OS_WIN32
  unsigned long bytes_ = 0;
#endif
};

inline udp::recv socket::recv(net::endpoint& endpoint, char* data, std::size_t size) {
  return { *this, endpoint, data, size };
}

inline udp::send socket::send(const net::endpoint& endpoint, const char* data, std::size_t size) {
  return { *this, endpoint, data, size };
}

inline udp::send_some socket::send_some(const net::endpoint& endpoint, const char* data, std::size_t size) {
  return { *this, endpoint, data, size };
}

}  // namespace ice::net::udp
