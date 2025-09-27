#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

class ComponentID {
public:
  template <typename T> static uint64_t get() {
    static const uint64_t id = next();
    return id;
  }

private:
  static uint64_t next() {
    static std::atomic_size_t counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed);
  }
};
