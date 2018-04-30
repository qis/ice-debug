#pragma once
#include <ice/config.h>
#include <ice/context.h>
#include <ice/handle.h>
#include <ice/net/endpoint.h>
#include <ice/net/option.h>
#include <iosfwd>
#include <limits>
#include <functional>
#include <cstdint>

namespace ice::net {

enum class shutdown {
  recv,
  send,
  both,
};

class socket {
public:
#if ICE_OS_WIN32
  struct close_type {
    void operator()(std::uintptr_t handle) noexcept;
  };
  using handle_type = ice::handle<std::uintptr_t, std::numeric_limits<std::uintptr_t>::max(), close_type>;
#else
  struct close_type {
    void operator()(int handle) noexcept;
  };
  using handle_type = ice::handle<int, -1, close_type>;
#endif
  using handle_view = handle_type::view;

  explicit socket(ice::context& context) noexcept : context_(context) {
  }

  socket(ice::context& context, int family, int type, int protocol = 0);

  socket(socket&& other) noexcept = default;
  socket& operator=(socket&& other) noexcept = default;

  socket(const socket& other) = delete;
  socket& operator=(const socket& other) = delete;

  virtual ~socket() = default;

  constexpr explicit operator bool() const noexcept {
    return handle_.valid();
  }

  void bind(const net::endpoint& endpoint);
  void shutdown(net::shutdown direction = net::shutdown::both);

  void close() noexcept {
    handle_.reset();
  }

  constexpr int family() const noexcept {
    return family_;
  }

  int type() const noexcept;
  int protocol() const noexcept;

  ice::error_code get(net::option& option) const noexcept {
    option.size() = option.capacity();
    return get(option.level(), option.name(), option.data(), option.size());
  }

  ice::error_code set(const net::option& option) noexcept {
    return set(option.level(), option.name(), option.data(), option.size());
  }

  ice::error_code get(int level, int name, void* data, socklen_t& size) const noexcept;
  ice::error_code set(int level, int name, const void* data, socklen_t size) noexcept;

  ice::context& context() noexcept {
    return context_.get();
  }

  const ice::context& context() const noexcept {
    return context_.get();
  }

  constexpr handle_type& handle() noexcept {
    return handle_;
  }

  constexpr const handle_type& handle() const noexcept {
    return handle_;
  }

  // Local endpoint on bound sockets.
  // Remote endpoint on connected or accepted sockets.
  constexpr net::endpoint& endpoint() noexcept {
    return endpoint_;
  }

  // Local endpoint on bound sockets.
  // Remote endpoint on connected or accepted sockets.
  constexpr const net::endpoint& endpoint() const noexcept {
    return endpoint_;
  }

protected:
  std::reference_wrapper<ice::context> context_;
  net::endpoint endpoint_;
  handle_type handle_;

private:
  int family_ = 0;
};

template <typename Char, typename Traits>
inline std::basic_ostream<Char, Traits>& operator<<(std::basic_ostream<Char, Traits>& os, const net::socket& s) {
  return os << s.endpoint();
}

}  // namespace ice::net
