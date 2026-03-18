#pragma once

#include <cstddef>
#include <cstdint>
#include <sys/mman.h>

#include <build.hpp>

namespace platform {
  const size_t PAGEBLOCK_SIZE = 64 * 1024;

  // Map region of arbitrary size 
  static void* allocate_region(size_t region) {
    void* ptr = mmap(nullptr, region, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ECS_ASSERT(ptr != MAP_FAILED, "Failed to allocate {} byte region.", region); 
    madvise(ptr, region, MADV_NOHUGEPAGE);
    return ptr;
  }

  static void unallocate_region(void* ptr, size_t region) {
    munmap(ptr, region);
  }

  // Page aligned block tied to a core local bump allocator.
  // A pageblock is defined by meta region defining activity and a data region holding SoA arrays. 
  struct alignas(64) PageBlock {
    PageBlock()
    : m_base(reinterpret_cast<uint8_t*>(allocate_region(PAGEBLOCK_SIZE))) {}

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
      }
      return *this;
    }

    // Disable copying (important!)
    PageBlock(const PageBlock&) = delete;
    PageBlock& operator=(const PageBlock&) = delete;

    uint8_t* m_base = nullptr;
    size_t m_waterline = 0;
    size_t m_count = 0;
  };
}

