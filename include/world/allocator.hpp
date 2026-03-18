#pragma once

#include "platform/pageblock.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <span>
#include <utility>
#include <vector>

namespace world {
  // Separated per cacheline, chunked arena bump allocator.  
  constexpr size_t CHUNK_SIZE = 64;
  constexpr size_t LOG2_CHUNK_SIZE = 6; 
  constexpr size_t THREAD_COUNT = 16; 

  inline size_t alignup(size_t x, size_t align) {
    return (x + align - 1) & ~(align - 1);
  }
  inline size_t aligndown(size_t x, size_t align) { return x & ~(align - 1); }

  template <typename T> 
  class alignas(64) Allocator {
    public:
      Allocator() {};
      ~Allocator() {};

      struct Chunk {
        size_t m_count = 0; // Amount of elements in chunk, full if == chunkCapacity. 
        T m_data[CHUNK_SIZE];
        Chunk* m_next; // Next chunk if full. 
      };

      T& push(T&& t) {
        if (m_lastChunk->m_count == CHUNK_SIZE) {
          m_lastChunk->m_next = new Chunk;
          m_count ++;
          m_lastChunk = m_lastChunk->m_next;
        } 

        m_lastChunk->m_data[m_lastChunk->m_count] = std::forward<T>(t);
        return m_lastChunk->m_data[m_lastChunk->m_count++];
      }

      size_t count() {return m_count;}
      T& get(size_t index) {
        size_t chunk = index >> LOG2_CHUNK_SIZE;
        size_t element = index & (CHUNK_SIZE - 1); 

        Chunk* base = m_base;
        for (int i = 0 ; i < chunk; i ++) {
          base = base->m_next;
        }
        
        return base->m_data[element];
      } // O(n) where N is the number of Chunks, iterate INDEX % CHUnk_SIZE.

    private: 
      size_t m_count = 0;
      Chunk* m_lastChunk = new Chunk; // Fast append path. 
      Chunk* m_base = m_lastChunk;
  };


 // Collection of PageBlocks belinging to a isngle Archetype (per thread). 
  struct ArchetypePool {
    Allocator<platform::PageBlock> m_blocks;
    size_t m_waterline = 0;

    size_t m_blockCap = 0;  
    size_t m_bitsetSize = 0;
    size_t m_count = 0;

    std::vector<size_t> m_offsets; 

    template <typename Arch>
    static ArchetypePool createPool() {
      ArchetypePool pool; 
      // Run initialisation code. 
      size_t lComp = Arch::canonical_types::ids[0].size;
      for (auto &id : Arch::canonical_types::ids) {
        if (id.size > lComp)
          lComp = id.size;
      }

      size_t reqESize = (Arch::size << 3) + 2; // Required size per entity in bits. (1 additional for activity bitset). 
      size_t lbCap = (platform::PAGEBLOCK_SIZE - (lComp * (2 + Arch::length)) << 3) / reqESize; // Lower bound capacity assuming 64 byte alignment per entity.

      pool.m_blockCap = lbCap;
      pool.m_bitsetSize = alignup(pool.m_blockCap / 8, 64); // Capacity / 8 for bitset size, aligned to a cache boundary.

      size_t alignedOffsets = 0;
      size_t index = 0;

      std::vector<size_t> canonicalOffsets;
      canonicalOffsets.reserve(Arch::length); 

      for (auto &id : Arch::canonical_types::ids) {
        canonicalOffsets.emplace_back(alignedOffsets);
        alignedOffsets += alignup(id.size * lbCap, 64);
        index++;
      }

      pool.m_offsets = std::move(canonicalOffsets);

      // ensure atleast m_waterline is valid. 
      pool.insertBlock();
      return pool;
    }

    template <typename ...Ts>
    inline size_t insert() {
      scan:
      platform::PageBlock& block = m_blocks.get(m_waterline);
      if (block.m_count < m_blockCap) {
        for (;;) {
          uint64_t* active = reinterpret_cast<uint64_t*>(block.m_base) + block.m_waterline;
          if (*active == UINT64_MAX) {block.m_waterline++; continue;}

          size_t index = __builtin_ctzll(~*active);
          *active |= (1ULL << index);
          block.m_count++;

          return index;
        }
      } else {
        // Insert a new block if waterline exceeds count. 
        if (++m_waterline >= m_count) insertBlock();
        goto scan;
      }
    } 

    inline void insertBlock() {
      platform::PageBlock block;
      std::memset(block.m_base, 0, m_bitsetSize * 2);
      m_blocks.push(std::move(block));
      m_count++;

      //std::cout << "Inserted new block: " << m_count << std::endl;
    }
  };

  class StorageArena {
    public:
      StorageArena() {};
      ~StorageArena() {};

      template <typename Arch, size_t Thread> 
      ArchetypePool& ensure_at() {
        static ArchetypePool& pool = m_pools[Thread].push(ArchetypePool::createPool<Arch>());
        return pool;
      } 

    private: 
      std::array<Allocator<ArchetypePool>, THREAD_COUNT> m_pools;
  };
}
