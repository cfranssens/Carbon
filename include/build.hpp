#pragma once

#include <format>

namespace util {

// Macro for clearly separating a debug impl from core.
#ifdef NDEBUG
#define DEBUG_TYPE(release_t, debug_t) release_t
#define DEBUG(statement)
#define NODEBUG(statement) statement
#else
#define DEBUG_TYPE(release_t, debug_t) debug_t
#define DEBUG(statement) statement
#define NODEBUG(statement)
#endif // !NDEBUG

namespace ansi {
constexpr const char *blue = "\033[34m";
constexpr const char *yellow = "\033[33m";
constexpr const char *reset = "\033[0m";
} // namespace ansi
#if defined(__GNUC__) || defined(__clang__)
#define ECS_TRAP() __builtin_trap()
#elif defined(_MSC_VER)
#define ECS_TRAP() __debugbreak()
#else
#define ECS_TRAP() std::abort()
#endif

#ifndef NDEBUG
#define ECS_ASSERT(expr, fmt, ...)                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      auto msg = std::format(fmt __VA_OPT__(, ) __VA_ARGS__);                  \
      std::fprintf(stderr,                                                     \
                   "\n\033[1;31m[ECS ASSERT]\033[0m\n"                         \
                   "  Expr: %s\n"                                              \
                   "  File: %s:%d\n"                                           \
                   "  Func: %s\n"                                              \
                   "  Msg : %s\n\n",                                           \
                   #expr, __FILE__, __LINE__, __func__, msg.c_str());          \
      ECS_TRAP();                                                              \
    }                                                                          \
  } while (0)
#else
#define ECS_ASSERT(expr, fmt, ...) ((void)0)
#endif
} // namespace util
