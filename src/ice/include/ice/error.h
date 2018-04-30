#pragma once
#include <iosfwd>
#include <string>
#include <system_error>
#include <type_traits>
#include <cassert>

namespace ice {

enum class errc : int {
  eof = 1,
  version,
};

enum class error_type : int {
  native = 0,
  system = 1 << 29,
  domain = 1 << 29 | 1 << 28,
};

// clang-format off

constexpr inline int error_category_mask =
  static_cast<int>(ice::error_type::native) |
  static_cast<int>(ice::error_type::system) |
  static_cast<int>(ice::error_type::domain);

// clang-format on

const std::error_category& native_category() noexcept;
const std::error_category& system_category() noexcept;
const std::error_category& domain_category() noexcept;

struct combined_t {};
constexpr inline combined_t combined;

class error_code {
public:
  constexpr error_code() noexcept = default;

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  constexpr error_code(T value) noexcept : value_(static_cast<int>(value)) {
    assert((value_ & error_category_mask) == static_cast<int>(ice::error_type::native));
  }

  constexpr error_code(std::errc code) noexcept :
    value_(static_cast<int>(code) | static_cast<int>(ice::error_type::system)) {
    assert((value_ & error_category_mask) == static_cast<int>(ice::error_type::system));
  }

  constexpr error_code(ice::errc code) noexcept :
    value_(static_cast<int>(code) | static_cast<int>(ice::error_type::domain)) {
    assert((value_ & error_category_mask) == static_cast<int>(ice::error_type::domain));
  }

  constexpr error_code(int code, ice::combined_t) noexcept : value_(code) {
  }

  constexpr explicit operator bool() const noexcept {
    return value_ != 0;
  }

  constexpr void clear() noexcept {
    value_ = 0;
  }

  [[nodiscard]] constexpr ice::error_type type() const noexcept {
    return static_cast<ice::error_type>(value_ & error_category_mask);
  }

  [[nodiscard]] constexpr int value() const noexcept {
    return value_ & ~error_category_mask;
  }

  [[nodiscard]] const std::error_category& category() const noexcept {
    switch (type()) {
    case ice::error_type::native: return ice::native_category();
    case ice::error_type::system: return ice::system_category();
    case ice::error_type::domain: return ice::domain_category();
    }
    return std::system_category();
  }

  [[nodiscard]] std::error_condition default_error_condition() const noexcept {
    return category().default_error_condition(value());
  }

  [[nodiscard]] std::string message() const { return default_error_condition().message(); }

    [[nodiscard]] constexpr int combined() const noexcept {
    return value_;
  }

private:
  int value_ = 0;
};

inline constexpr bool operator==(const ice::error_code& lhs, const ice::error_code& rhs) noexcept {
  return lhs.combined() == rhs.combined();
}

inline constexpr bool operator!=(const ice::error_code& lhs, const ice::error_code& rhs) noexcept {
  return !(lhs == rhs);
}

inline constexpr bool operator==(const ice::error_code& lhs, ice::errc rhs) noexcept {
  return lhs.type() == error_type::domain && lhs.value() == static_cast<int>(rhs);
}

inline constexpr bool operator!=(const ice::error_code& lhs, ice::errc rhs) noexcept {
  return !(lhs == rhs);
}

inline constexpr bool operator==(ice::errc lhs, const ice::error_code& rhs) noexcept {
  return rhs == lhs;
}

inline constexpr bool operator!=(ice::errc lhs, const ice::error_code& rhs) noexcept {
  return rhs != lhs;
}

inline constexpr bool operator==(const ice::error_code& lhs, std::errc rhs) noexcept {
  return lhs.type() == error_type::system && lhs.value() == static_cast<int>(rhs);
}

inline constexpr bool operator!=(const ice::error_code& lhs, std::errc rhs) noexcept {
  return !(lhs == rhs);
}

inline constexpr bool operator==(std::errc lhs, const ice::error_code& rhs) noexcept {
  return rhs == lhs;
}

inline constexpr bool operator!=(std::errc lhs, const ice::error_code& rhs) noexcept {
  return rhs != lhs;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline constexpr bool operator==(const ice::error_code& lhs, T rhs) noexcept {
  return lhs.type() == error_type::native && lhs.value() == static_cast<int>(rhs);
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline constexpr bool operator!=(const ice::error_code& lhs, T rhs) noexcept {
  return !(lhs == rhs);
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline constexpr bool operator==(T lhs, const ice::error_code& rhs) noexcept {
  return rhs == lhs;
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
inline constexpr bool operator!=(T lhs, const ice::error_code& rhs) noexcept {
  return rhs != lhs;
}

template <typename Char, typename Traits>
inline std::basic_ostream<Char, Traits>& operator<<(std::basic_ostream<Char, Traits>& os, const ice::error_code& ec) {
  os << ec.message().data();
  if (ec.type() == ice::error_type::native) {
    os << " (" << ec.value() << ')';
  }
  return os;
}

class system_error : public std::system_error {
public:
  system_error(std::error_code ec) : std::system_error(ec) {
  }

  system_error(std::error_code ec, const std::string& what) : std::system_error(ec, what) {
  }

  system_error(std::error_code ec, const char* what) : std::system_error(ec, what) {
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  system_error(T ev, const std::error_category& category) : std::system_error(static_cast<int>(ev), category) {
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  system_error(T ev, const std::error_category& category, const std::string& what) :
    std::system_error(static_cast<int>(ev), category, what) {
  }

  template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
  system_error(T ev, const std::error_category& category, const char* what) :
    std::system_error(static_cast<int>(ev), category, what) {
  }

  system_error(ice::error_code ec) : system_error(ec.value(), ec.category()) {
  }

  system_error(ice::error_code ec, const std::string& what) : system_error(ec.value(), ec.category(), what) {
  }

  system_error(ice::error_code ec, const char* what) : system_error(ec.value(), ec.category(), what) {
  }
};

}  // namespace ice
