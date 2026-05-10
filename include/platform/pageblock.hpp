#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#if !defined(__EMSCRIPTEN__)
#include <sys/mman.h>
#endif

#include <build.hpp>

namespace platform {

  constexpr size_t PAGEBLOCK_SIZE = 1024 * 1024;

  // Map region of arbitrary size
  static void* allocate_region(size_t region) {

    #if defined(__EMSCRIPTEN__)

    void* ptr = std::malloc(region);

    if (!ptr) {
      printf("malloc failed\n");
      abort();
    }

    return ptr;

    #else

    void* ptr = mmap(
      nullptr,
      region,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS,
      -1,
      0
    );

    if (ptr == MAP_FAILED) {
      printf("mmap failed\n");
      abort();
    }

    return ptr;

    #endif
  }

  static void unallocate_region(void* ptr, size_t region) {

    #if defined(__EMSCRIPTEN__)

    std::free(ptr);

    #else

    munmap(ptr, region);

    #endif
  }

  // Page aligned block tied to a core local bump allocator.
  // A pageblock is defined by meta region defining activity
  // and a data region holding SoA arrays.
  struct alignas(64) PageBlock {

    PageBlock()
    : m_base(reinterpret_cast<uint8_t*>(
      allocate_region(PAGEBLOCK_SIZE))) {}

      ~PageBlock() {
        if (m_base)
          unallocate_region(m_base, PAGEBLOCK_SIZE);
      }

      // Move constructor
      PageBlock(PageBlock&& other) noexcept
      : m_base(other.m_base),
      m_waterline(other.m_waterline),
      m_count(other.m_count) {

        other.m_base = nullptr;
        other.m_waterline = 0;
        other.m_count = 0;
      }

      // Move assignment
      PageBlock& operator=(PageBlock&& other) noexcept {

        if (this != &other) {

          if (m_base)
            unallocate_region(m_base, PAGEBLOCK_SIZE);

          m_base = other.m_base;
          m_waterline = other.m_waterline;
          m_count = other.m_count;

          other.m_base = nullptr;
          other.m_waterline = 0;
          other.m_count = 0;
        }

        return *this;
      }

      // Disable copying
      PageBlock(const PageBlock&) = delete;
      PageBlock& operator=(const PageBlock&) = delete;

      uint8_t* m_base = nullptr;
      size_t m_waterline = 0;
      size_t m_count = 0;
  };

}
