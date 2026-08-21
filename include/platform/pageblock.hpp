#pragma once 

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if __has_include(<sys/mman.h>)
#include <sys/mman.h>
#define PLATFORM_HAS_MMAP 1
#else
#define PLATFORM_HAS_MMAP 0
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
#define MADV_HUGEPAGE 14
#endif

namespace platform {
  static void* allocate_region(size_t region) {
    void* ptr = nullptr;

#if PLATFORM_HAS_MMAP
    ptr = mmap(
      nullptr,
      region,
      PROT_READ | PROT_WRITE,
      MAP_PRIVATE | MAP_ANONYMOUS,
      -1,
      0
    );

    if (ptr == MAP_FAILED) {
      ptr = nullptr;
    } else {
      madvise(ptr, region, MADV_HUGEPAGE);
    }
#endif

    // Fallback to standard malloc if mmap is unavailable or failed
    if (ptr == nullptr) {
      ptr = std::malloc(region);
    }

    if (ptr == nullptr) {
      printf("Allocation failed\n");
      abort();
    }

    return ptr;
  } 

  struct PageBlock {
    // meta
    size_t m_count = 0;
    size_t m_waterline = 0; // Word index till where the bitset is completely saturated. 

    // Only ever allocate in one block size. While interaction with different sized blocks, 
    // for example in the context of networking is possible; it is not possible to allocate these natively.  
    PageBlock(size_t clearRegion = 0) {
      if (clearRegion > 0) {
          std::memset(data(), 0, clearRegion);
      }
    }; 
   
    std::byte* data() {
        return reinterpret_cast<std::byte*>(this) + sizeof(PageBlock);
    }

    void clear(uint64_t bitsetRegion) {
        m_waterline = 0;
        m_count = 0;
        
        std::memset(data(), 0, bitsetRegion);
    }
  };
}
