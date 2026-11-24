#pragma once 

#include "carbon/world/archetype.hpp"
#include <carbon/platform/allocator.hpp>

#include <cstdint>
#include <mutex>
#include <sys/mman.h>

namespace world {
  constexpr size_t BLOCK_SIZE = 32 * 1024; 
  constexpr size_t THREAD_COUNT = 8;
  using PagedAllocator = platform::ArenaAllocator<BLOCK_SIZE>;
  
  // Storage object belonging to a single archetype 
  template <typename World> class ArchetypeStorage {
    public:
      ArchetypeStorage() {
        m_storages = reinterpret_cast<uint8_t*>(mmap(nullptr, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
      };

      PagedAllocator* allocator(size_t index) {
        size_t block = index >> ALLOCATOR_BLOCK_CAPACITY_SHIFT;
        size_t shift = index & (ALLOCATOR_BLOCK_CAPACITY - 1);
        
        return *reinterpret_cast<PagedAllocator**>(m_storages + (block * BLOCK_SIZE) + (shift * sizeof(PagedAllocator*))); 
      }

      // Call statically 
      template <typename Arch, size_t Thread> PagedAllocator* allocator() {
        static_assert(Thread < THREAD_COUNT, "Thread index exceeds THREAD_COUNT");
        static size_t archetypeIndex = world::getLocalArchetypeIndex<Arch, World>() * THREAD_COUNT + Thread;

        typedef typename std::aligned_storage<sizeof(PagedAllocator), alignof(PagedAllocator)>::type Storage;
        static Storage storage;                                            // static storage honors alignas(64)
        static PagedAllocator* alloc = new (static_cast<void*>(&storage)) PagedAllocator;   // needs <new>

        const size_t block      = archetypeIndex >> ALLOCATOR_BLOCK_CAPACITY_SHIFT;
        const size_t blockIndex = archetypeIndex & (ALLOCATOR_BLOCK_CAPACITY - 1);

        ++m_archCount;
        if (block >= m_blockCount) {
        void* p = mremap(m_storages, m_blockCount * BLOCK_SIZE, (block + 1) * BLOCK_SIZE, MREMAP_MAYMOVE);
          if (p == MAP_FAILED) std::abort();
          m_storages = static_cast<uint8_t*>(p);
          m_blockCount = block + 1;
        }
        reinterpret_cast<PagedAllocator**>(m_storages + block * BLOCK_SIZE)[blockIndex] = alloc;
        return alloc;
      }

    private: 
      uint8_t* m_storages = nullptr;
      size_t m_blockCount = 1;
      size_t m_archCount = 0; 

      // sizeof(PagedAllocator)* == 8) 
     // ALLOCATOR_BLOCK_CAPACITY = POT when BLOCK_SIZE = POT
      constexpr static size_t ALLOCATOR_BLOCK_CAPACITY = BLOCK_SIZE / sizeof(PagedAllocator*);
      constexpr static size_t ALLOCATOR_BLOCK_CAPACITY_SHIFT = platform::log2_constexpr(ALLOCATOR_BLOCK_CAPACITY);
      static_assert(ALLOCATOR_BLOCK_CAPACITY_SHIFT > 6, "BLOCK_SIZE should be larger than one (1) PagedAllocator (64b)!");
  };
}
