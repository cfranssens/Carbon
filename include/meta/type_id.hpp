#pragma once

#include "build.hpp"
#include <cstddef>
#include <cstdint>

#include <format>
#include <string_view>
#include <type_traits>

namespace world::meta {
// Representation of a type.
struct type_id_t {
  const uint64_t hash;
  const bool is_const;
  const size_t size;

  DEBUG(std::string_view type_name;)

  constexpr bool operator==(const type_id_t &o) const noexcept {
    return hash == o.hash;
  }
  constexpr bool operator<(const type_id_t &o) const noexcept {
    return hash < o.hash;
  }
  constexpr bool operator>(const type_id_t &o) const noexcept {
    return hash > o.hash;
  }
};

// Pointer hash for constexpr ordering.
constexpr uint64_t ct_hash(const char *s) {
  uint64_t h = 1469598103834665603ull;
  while (*s) {
    h ^= uint64_t(*s++);
    h *= 1099511628211ull;
  }
  return h;
}

#ifdef NDEBUG
// Type id generator (GNU/Linux)
template <typename T> inline constexpr static type_id_t type_id() {
  return type_id_t{ct_hash(__PRETTY_FUNCTION__), std::is_const_v<T>, sizeof(T)};
}
#endif
} // namespace world::meta

#ifndef NDEBUG

template <> struct std::formatter<world::meta::type_id_t, char> {
  constexpr auto parse(std::format_parse_context &ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(world::meta::type_id_t t, FormatContext &ctx) const {
    return std::format_to(ctx.out(), "{}{}{}",
                          t.is_const ? util::ansi::yellow : util::ansi::blue,
                          t.type_name, util::ansi::reset);
  }
};

namespace world::meta {
template <typename T> constexpr std::string_view type_name_sv() {
#if defined(__clang__) || defined(__GNUC__)
  constexpr std::string_view fn = __PRETTY_FUNCTION__;
  constexpr std::string_view key = "T = ";

  const auto start = fn.find(key) + key.size();
  const auto end = fn.find(';', start); // <-- critical fix

  return fn.substr(start, end - start);

#elif defined(_MSC_VER)
  constexpr std::string_view fn = __FUNCSIG__;
  constexpr std::string_view prefix = "type_name_sv<";
  constexpr std::string_view suffix = ">(void)";

  const auto start = fn.find(prefix) + prefix.size();
  const auto end = fn.rfind(suffix);

  return fn.substr(start, end - start);
#endif
}

// Type id generator (GNU/Linux)
template <typename T> inline constexpr static type_id_t type_id() {
  return type_id_t{ct_hash(__PRETTY_FUNCTION__), std::is_const_v<T>, sizeof(T),
                   type_name_sv<T>()};
}
} // namespace world::meta
#endif
