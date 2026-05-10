#pragma once

#include "archetype.hpp"
#include "build.hpp"
#include "platform/pageblock.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <utility>
#include <vector>

namespace world {
  // Separated per cacheline, chunked arena bump allocator.  
  constexpr size_t CHUNK_SIZE = 16;
  constexpr size_t LOG2_CHUNK_SIZE = 4;
  constexpr size_t THREAD_COUNT = 1;

  inline size_t alignup(size_t x, size_t align) {
    return (x + align - 1) & ~(align - 1);
  }
  inline size_t aligndown(size_t x, size_t align) { return x & ~(align - 1); }

  template <typename T> 
  class alignas(64) Allocator {
    public:
      Allocator() {};
      ~Allocator() { // TODO! properly ensure memory is cleared.
        for (auto* c : m_chunks) {
          //delete c;
        }
      };

      struct Chunk {
        size_t m_count = 0; // Amount of elements in chunk, full if == chunkCapacity. 
        T m_data[CHUNK_SIZE];
      };

      T& push(T&& t) {
        if (m_lastChunk->m_count == CHUNK_SIZE) {
          m_count ++;
          m_chunks.emplace_back(new Chunk);
          m_lastChunk = m_chunks.back();
        } 
  
        m_lastChunk->m_data[m_lastChunk->m_count] = std::forward<T>(t);
        return m_lastChunk->m_data[m_lastChunk->m_count++];
      }

      size_t count() {return m_count;}

      // AHHHHHHH
      T& get(size_t index) {
        size_t chunk = index >> LOG2_CHUNK_SIZE;
        size_t element = index & (CHUNK_SIZE - 1); 
        
        Chunk* c = m_chunks[chunk];
        return c->m_data[element];
      } 

    private: 
      std::vector<Chunk*> m_chunks {new Chunk}; // Pointer table

      size_t m_count = 0;
      Chunk* m_lastChunk = m_chunks.back(); // Fast append path. 
      Chunk* m_base = m_lastChunk;
  };


 // Collection of PageBlocks belinging to a isngle Archetype (per thread). 
  struct ArchetypePool {
    Allocator<platform::PageBlock> m_blocks;
    size_t m_waterline = 0;

    size_t m_blockCap = 0;  
    size_t m_bitsetSize = 0;
    size_t m_count = 0;

    size_t m_archetypeIndex;
    std::vector<size_t> m_offsets; 

    template <typename Arch>
    static ArchetypePool createPool(size_t poolIndex) {
      ArchetypePool pool; 

      // Find largest element for alignment
      size_t lComp = Arch::canonical_types::ids[0].size;
      for (auto &id : Arch::canonical_types::ids) {
        if (id.size > lComp)
          lComp = id.size;
      }

      size_t reqESize = (Arch::size << 3) + 2; // Required size per entity in bits. (1 additional for activity bitset). 
      size_t nArrays = Arch::length + 2;
      size_t bitPadding = (63 * nArrays * 8) + 7;
      size_t lbCap = ((platform::PAGEBLOCK_SIZE << 3) - bitPadding) / reqESize; // Lower bound capacity assuming 64 byte alignment per entity.

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

    template <size_t Thread, typename ...Ts>
    inline uint64_t insert(Ts&&... components) {
      using Canonical = Archetype<Ts...>::canonical_types;
      scan:
      platform::PageBlock& block = m_blocks.get(m_waterline);

      if (block.m_count < m_blockCap) {
        for (;;) {
          uint64_t* __restrict active = reinterpret_cast<uint64_t*>(block.m_base) + block.m_waterline;
          while (*active == UINT64_MAX) {active++;}

          uint64_t word = *active;
          uint64_t inv = ~word;
          uint64_t bit = inv & -inv;        // isolate lowest zero bit
          *active = word | bit;

          int index = __builtin_ctzll(bit);
          block.m_waterline = active - reinterpret_cast<uint64_t*>(block.m_base);

          ECS_ASSERT(m_waterline < (1ULL<<24) && index < UINT32_MAX, "Exceeds entity index bounds: ({}, {})", m_waterline, index);
          auto values = std::tuple<std::decay_t<Ts>...>(std::forward<Ts>(components)...);

          world::meta::for_each_type_indexed<Canonical>::apply([&]<typename T, size_t I>() {
            auto &value = std::get<T>(values);
            *(T*)(block.m_base + (m_bitsetSize * 2) + m_offsets[I] + (index * sizeof(T))) = value;
          });

          // Entitites in the block
          block.m_count++;

          // [THREAD(8)][Archetype(16)][Block(16)][Index(24)]
          return (Thread << 56) | (m_archetypeIndex << 40) | (m_waterline << 24) | index; // Combine 
        }
      } else {
        // Insert a new block if waterline exceeds count. 
        if (++m_waterline >= m_count) insertBlock();
        goto scan;
      }
    }

    // Does NOT take thread nor archetype into account, will just pop this id from the current pool if called. 
    inline void removeEntity(uint64_t entity) {
      size_t blockIndex = (entity >> 24) & ((1ULL << 16) - 1);
      size_t index = entity & ((1ULL << 24) - 1);
      size_t wordIndex = index >> 6;
    
      platform::PageBlock& block = m_blocks.get(blockIndex);
      uint64_t* active = reinterpret_cast<uint64_t*>(block.m_base) + wordIndex;
      *active &= ~(1Ull << (index & 63));
      
      block.m_waterline = std::min(block.m_waterline, wordIndex);
      m_waterline = std::min(m_waterline, blockIndex);
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
        static ArchetypePool& pool = m_pools[Thread].push(ArchetypePool::createPool<Arch>(m_pools[Thread].count()));
        m_signatures[Thread].push_back(Arch::signature);
        return pool;
      } 

      // Retrieve from a generated index 
      ArchetypePool& get(size_t thread, size_t index) {
        ECS_ASSERT(thread < THREAD_COUNT && index < m_pools[thread].count(), "Index out of bounds.");
        return m_pools[thread].get(index);
      }

    private: 
      std::array<Allocator<ArchetypePool>, THREAD_COUNT> m_pools;
      // Parallel structure to the underlying pools, to avoid fragmentation whhen scanning. 
      std::array<std::vector<Signature>, THREAD_COUNT> m_signatures;
  };
}
