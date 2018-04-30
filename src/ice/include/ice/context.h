#pragma once
#include <ice/config.h>
#include <ice/error.h>
#include <ice/event.h>
#include <ice/handle.h>
#include <ice/utility.h>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace ice {

class schedule;

class context {
public:
#if ICE_OS_WIN32
  struct close_type {
    void operator()(std::uintptr_t handle) noexcept;
  };
  using handle_type = ice::handle<std::uintptr_t, 0, close_type>;
#else
  struct close_type {
    void operator()(int handle) noexcept;
  };
  using handle_type = ice::handle<int, -1, close_type>;
#endif
  using handle_view = handle_type::view;

  constexpr static std::uint32_t stop_requested_flag = 1;
  constexpr static std::uint32_t thread_count_increment = 2;

  context();

  context(context&& other) = delete;
  context& operator=(context&& other) = delete;

  context(const context& other) = delete;
  context& operator=(const context& other) = delete;

  virtual ~context();

  void run(std::size_t event_buffer_size = 1);

  void interrupt() noexcept;
  bool stop() noexcept;

  ice::schedule schedule(bool queue = false);

  constexpr handle_type& handle() noexcept {
    return handle_;
  }

  constexpr const handle_type& handle() const noexcept {
    return handle_;
  }

#if ICE_OS_LINUX
  constexpr handle_type& events() noexcept {
    return events_;
  }

  constexpr const handle_type& events() const noexcept {
    return events_;
  }
#endif

  bool is_current() const noexcept {
    return index_.get() ? true : false;
  }

private:
  std::atomic_uint32_t state_ = 0;
  ice::thread_local_storage index_;
  handle_type handle_;
#if ICE_OS_LINUX
  handle_type events_;
#endif
};

class schedule final : public ice::event {
public:
#if ICE_OS_LINUX
  schedule(ice::context& context, bool queue) noexcept :
    context_(context.handle()), handle_(context.events()), ready_(!queue && context.is_current()) {
  }
#else
  schedule(ice::context& context, bool queue) noexcept :
    context_(context.handle()), ready_(!queue && context.is_current()) {
  }
#endif

  constexpr bool await_ready() const noexcept {
    return ready_;
  }

  bool suspend() noexcept override;

  bool resume() noexcept override {
    return true;
  }

  void await_resume() {
    if (ec_) {
      throw ice::system_error(ec_, "schedule");
    }
  }

private:
  context::handle_view context_;
#if ICE_OS_LINUX
  context::handle_view handle_;
#endif
  const bool ready_;
};

inline ice::schedule context::schedule(bool queue) {
  return { *this, queue };
}

}  // namespace ice
