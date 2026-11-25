#pragma once

#include "carbon/platform/pageblock.hpp"
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <iostream>
#include <mutex>
#include <sys/mman.h>

namespace platform {
  constexpr size_t WORD_LENGTH = sizeof(size_t) << 3;
  constexpr size_t WORD_SHIFT = __builtin_ctzll(WORD_LENGTH);
  constexpr size_t align_up(size_t val, size_t alignment) {
    return (val + alignment - 1) & ~(alignment - 1);
  }

  constexpr size_t log2_constexpr(size_t n) {
    return (n <= 1) ? 0 : 1 + log2_constexpr(n >> 1);
  }

  // BLOCK_SIZE should preferable be aligned to multiples of platform's page size. 
  template <size_t BLOCK_SIZE> class alignas(WORD_LENGTH) ArenaAllocator {
    public: 
      ArenaAllocator() {
        // Ensure zeroed out 
        m_meta = reinterpret_cast<uint8_t*>(mmap(nullptr, BLOCK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        m_metaRegionCount++;
      }

      ~ArenaAllocator() {
      }

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
                reinterpret_cast<uint8_t**>(metaBlock->data() + META_BLOCK_BITSET_OFFSET)[metaBlock->m_waterline] = reinterpret_cast<uint8_t*>(mmap(nullptr, REGION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_POPULATE, -1, 0));
              }

              size_t shift = __builtin_ctzll(~word);
              word |= size_t(1) << shift; // Immediate write. 

              m_currentBlock = m_metaWaterline * TRACKED_REGIONS_PER_META_BLOCK + (metaBlock->m_waterline << WORD_SHIFT) + shift;
              uint8_t* regionStart = reinterpret_cast<uint8_t**>(metaBlock->data() + META_BLOCK_BITSET_OFFSET)[metaBlock->m_waterline];  
              
              return reinterpret_cast<PageBlock*>(regionStart + (shift * BLOCK_SIZE));
            }
          }

          expandMetaRegion();
        }
      }
       
      // Linearly grow meta region by one block size  
      void expandMetaRegion() {
        //std::lock_guard<std::mutex> mutex(m_mutex);
        // Replace this with a POSIX compliant call
        m_meta = reinterpret_cast<uint8_t*>(mremap(m_meta, m_metaRegionCount * BLOCK_SIZE, (m_metaRegionCount + 1) * BLOCK_SIZE, MREMAP_MAYMOVE));
        m_metaRegionCount++;
        // std::cout << "Expanded" << std::endl;
      }

      PageBlock* at(size_t flat) const {
        size_t metaIdx = flat / TRACKED_REGIONS_PER_META_BLOCK;
        size_t inMeta  = flat % TRACKED_REGIONS_PER_META_BLOCK;
        size_t word    = inMeta >> WORD_SHIFT;
        size_t bit     = inMeta & ((size_t(1) << WORD_SHIFT) - 1);
        PageBlock* mb  = reinterpret_cast<PageBlock*>(m_meta + metaIdx * BLOCK_SIZE);
        uint8_t* region = reinterpret_cast<uint8_t**>(mb->data() + META_BLOCK_BITSET_OFFSET)[word];
        return reinterpret_cast<PageBlock*>(region + bit * BLOCK_SIZE);
      }

      uint8_t* m_meta = nullptr; // no std::byte on c++11, BLOCK_SIZE aligned contigious array. 
      size_t m_metaRegionCount = 0; 
      size_t m_metaWaterline = 0;  
      size_t m_currentBlock = 0; // Last touched block index
      
      // Externally managed 
      size_t m_cursor = 0; // Earliest index with sufficient free space 
      size_t m_capacity = 0; // Per block capacity 

      constexpr static size_t REGION_SPAN = WORD_LENGTH; // Allows tracking with a single size_t
      constexpr static size_t REGION_SIZE = REGION_SPAN * BLOCK_SIZE;
      constexpr static size_t TRACKED_REGIONS_PER_META_BLOCK = BLOCK_SIZE / (sizeof(uint8_t*) + sizeof(size_t)); // Pointer + bitset 
      constexpr static size_t META_BLOCK_BITSET_OFFSET = align_up(TRACKED_REGIONS_PER_META_BLOCK >> 3, WORD_LENGTH); 
    
      class iterator {
        public:
          typedef std::forward_iterator_tag iterator_category;
          typedef PageBlock*                value_type;
          typedef std::ptrdiff_t            difference_type;
          typedef PageBlock* const*         pointer;
          typedef PageBlock* const&         reference;
 
          iterator() : m_arena(nullptr), m_metaIdx(0), m_wordIdx(0), m_bits(0), m_region(nullptr), m_cur(nullptr) {}
 
          reference operator*()  const { return m_cur; }
          pointer   operator->() const { return &m_cur; }
 
          iterator& operator++()    { m_bits &= m_bits - 1; settle(); return *this; }  // clear lowest set bit, move on
          iterator  operator++(int) { iterator t(*this); ++*this; return t; }
 
          bool operator==(const iterator& o) const { return m_metaIdx == o.m_metaIdx && m_wordIdx == o.m_wordIdx && m_bits == o.m_bits; }
          bool operator!=(const iterator& o) const { return !(*this == o); }
 
          // Flat block index.
          size_t index() const {
            return m_metaIdx * TRACKED_REGIONS_PER_META_BLOCK + (m_wordIdx << WORD_SHIFT) + size_t(__builtin_ctzll(m_bits));
          }
 
          private:
            friend class ArenaAllocator;
            iterator(const ArenaAllocator* a, size_t metaIdx) : m_arena(a), m_metaIdx(metaIdx), m_wordIdx(0), m_bits(0), m_region(nullptr), m_cur(nullptr) {}
 
            void load() {
              if (m_metaIdx >= m_arena->m_metaRegionCount) { m_bits = 0; m_region = nullptr; return; }
              PageBlock* mb = reinterpret_cast<PageBlock*>(m_arena->m_meta + m_metaIdx * BLOCK_SIZE);
              m_bits   = reinterpret_cast<size_t*>(mb->data())[m_wordIdx];
              m_region = reinterpret_cast<uint8_t**>(mb->data() + META_BLOCK_BITSET_OFFSET)[m_wordIdx];
            }
 
            // Skip empty.
            void settle() {
              while (m_bits == 0) {
                if (m_metaIdx >= m_arena->m_metaRegionCount) { m_cur = nullptr; return; }   // end
                if (++m_wordIdx >= (TRACKED_REGIONS_PER_META_BLOCK >> WORD_SHIFT)) { m_wordIdx = 0; ++m_metaIdx; }
                load();
              }
              m_cur = reinterpret_cast<PageBlock*>(m_region + size_t(__builtin_ctzll(m_bits)) * BLOCK_SIZE);
            }
 
            const ArenaAllocator* m_arena;
            size_t   m_metaIdx;   // meta block
            size_t   m_wordIdx;   // region
            size_t   m_bits;      // current m_bits in region  
            uint8_t* m_region;    // region start for the current word
            PageBlock* m_cur;
          };
 
          iterator begin() const { iterator it(this, 0); it.load(); it.settle(); return it; }
          iterator end()   const { return iterator(this, m_metaRegionCount); } 
    };
}
