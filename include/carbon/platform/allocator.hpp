#include "carbon/platform/pageblock.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <iostream>
#include <sys/mman.h>

namespace platform {
  constexpr size_t WORD_LENGTH = sizeof(size_t) << 3;
  constexpr size_t WORD_SHIFT = __builtin_ctzll(WORD_LENGTH);

  constexpr size_t align_up(size_t val, size_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
  }


  // BLOCK_SIZE should preferable be aligned to multiples of platform's page size. 
  template <size_t BLOCK_SIZE> class alignas(WORD_LENGTH) ArenaAllocator {
    public: 
      ArenaAllocator() {
        // Ensure zeroed out 
        m_meta = reinterpret_cast<uint8_t*>(mmap(nullptr, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        m_metaRegionCount++;
      }

      ~ArenaAllocator() {}

      // Allocate a new BLOCK_SIZE region
      PageBlock* allocate() {
        // Monotonic iteration
        for (;;) {
          for (; m_metaWaterline < m_metaRegionCount; m_metaWaterline++) {
            PageBlock* metaBlock = reinterpret_cast<PageBlock*>(m_meta + (m_metaWaterline * BLOCK_SIZE));
            for (; metaBlock->m_waterline < (TRACKED_REGIONS_PER_META_BLOCK >> WORD_SHIFT); metaBlock->m_waterline++) {
              size_t& word = reinterpret_cast<size_t*>(metaBlock->data())[metaBlock->m_waterline];
              if (~word == 0) continue;
              
              // Unallocated region 
              if (word == 0) {
                reinterpret_cast<uint8_t**>(metaBlock->data() + META_BLOCK_BITSET_OFFSET)[metaBlock->m_waterline] = reinterpret_cast<uint8_t*>(mmap(nullptr, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
              }

              size_t shift = __builtin_ctzll(~word);
              word |= size_t(1) << shift;

              //size_t blockIndex = m_metaWaterline * TRACKED_REGIONS_PER_META_BLOCK + (metaBlock->m_waterline << WORD_SHIFT) + shift;
              uint8_t* regionStart = reinterpret_cast<uint8_t**>(metaBlock->data() + META_BLOCK_BITSET_OFFSET)[metaBlock->m_waterline];  
            
              return reinterpret_cast<PageBlock*>(regionStart + (shift * BLOCK_SIZE));
            }
          }

          expandMetaRegion();
        }
      }

    public: 
      // Linearly grow meta region by one block size  
      void expandMetaRegion() {
        // Replace this with a POSIX compliant call
        m_meta = reinterpret_cast<uint8_t*>(mremap(m_meta, m_metaRegionCount * BLOCK_SIZE, (m_metaRegionCount + 1) * BLOCK_SIZE, MREMAP_MAYMOVE));
        m_metaRegionCount++;
        // std::cout << "Expanded" << std::endl;
      }

      uint8_t* m_meta = nullptr; // no std::byte on c++11, BLOCK_SIZE aligned contigious array. 
      size_t m_metaRegionCount = 0; 
      size_t m_metaWaterline = 0;   

      constexpr static size_t REGION_SPAN = WORD_LENGTH; // Allows tracking with a single size_t
      constexpr static size_t REGION_SIZE = REGION_SPAN * BLOCK_SIZE;
      constexpr static size_t TRACKED_REGIONS_PER_META_BLOCK = BLOCK_SIZE / (sizeof(uint8_t*) + sizeof(size_t)); // Pointer + bitset 
      constexpr static size_t META_BLOCK_BITSET_OFFSET = align_up(TRACKED_REGIONS_PER_META_BLOCK >> 3, WORD_LENGTH); 
    };
}
