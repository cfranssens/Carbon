#pragma once 


#include "meta/type_id.hpp"
#include "meta/type_list.hpp"
#include "platform/pageblock.hpp"
#include "world/archetype.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>
#include <array>

namespace world {  
 // Arema allocator to ensure stable references.
  // Memory within a Chunk is contigious but not guaranteed betwwen chunks. 
  template<typename T> class ArenaAllocator {
  private:
      struct Chunk {
        std::byte* m_data;
        size_t m_chunkBytes;
        size_t m_used = 0;

        // Ensure alignment
        explicit Chunk(size_t capacity, size_t increment) : m_chunkBytes(capacity * increment) {
          m_data = static_cast<std::byte*>(platform::allocate_region(m_chunkBytes)); 
        }

        ~Chunk() {
          munmap(m_data, m_chunkBytes);
        }
      
        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;
      };

      // Chunk capacity is global per allocator. Needed to support multiple capacities between worlds/hardware. 
      size_t m_increment; // Stride 
      
      struct ThreadContext {
        std::vector<Chunk*> m_chunks;
        Chunk* m_currentChunk = nullptr; // Shorthand, 
        size_t m_count = 0; 
      };

      std::array<ThreadContext, THREAD_COUNT> m_threadedChunks;

    public:
      explicit ArenaAllocator(size_t increment = sizeof(T)) : m_increment(increment) {
        // Allocate first chunk
       // m_currentChunk = m_chunks.emplace_back(new Chunk(m_chunkCapacity, m_increment));
      }

      ArenaAllocator(const ArenaAllocator&) = delete;
      ArenaAllocator& operator=(const ArenaAllocator&) = delete;

      ArenaAllocator(ArenaAllocator&&) noexcept = default;
      ArenaAllocator& operator=(ArenaAllocator&&) noexcept = default;

      ~ArenaAllocator() {

        for (ThreadContext& context : m_threadedChunks) {
          for (Chunk* chunk : context.m_chunks) {
            for (size_t i = 0; i < chunk->m_used; ++i) {
              T* obj = reinterpret_cast<T*>(&chunk->m_data[i * m_increment]);
              obj->~T();
            }
          }
        }
      }
  
      template<size_t Thread> T* at(size_t index) {
        return reinterpret_cast<T*>(&m_threadedChunks[Thread].m_chunks[index >> LOG2_CHUNK_CAP]->m_data[(index & (ALLOCATOR_CHUNK_CAP - 1)) * m_increment]);
      }
      T* at(size_t thread, size_t index) {
        return reinterpret_cast<T*>(&m_threadedChunks[thread].m_chunks[index >> LOG2_CHUNK_CAP]->m_data[(index & (ALLOCATOR_CHUNK_CAP - 1)) * m_increment]);
      }
      
      template<size_t Thread> T* back() {
        return reinterpret_cast<T*>(&m_threadedChunks[Thread].m_currentChunk->m_data[m_threadedChunks[Thread].m_currentChunk->m_used - 1 * m_increment]);
      }     

      template<size_t Thread> size_t count() {
        return m_threadedChunks[Thread].m_count;
      }

      template<size_t Thread, typename... Args> T* create(Args&&... args) { 
        auto& threadContext = m_threadedChunks[Thread];
        // Needed to ensure unused threads dont allocate chunks, initial chunks. 
        // Should be stable with a const Args... layout across calls. 
        [[unlikely]] if (threadContext.m_currentChunk == nullptr || threadContext.m_currentChunk->m_used >= ALLOCATOR_CHUNK_CAP) {
          threadContext.m_currentChunk = threadContext.m_chunks.emplace_back(
            new Chunk(ALLOCATOR_CHUNK_CAP, m_increment)
          );
        }

        T* ptr = reinterpret_cast<T*>(&threadContext.m_currentChunk->m_data[threadContext.m_currentChunk->m_used * m_increment]);
        new (ptr) T(std::forward<Args>(args)...);
        threadContext.m_currentChunk->m_used++;
        threadContext.m_count++;
              
        return ptr;
        
      }

      class ThreadIterator {
        private:
          const std::vector<Chunk*>* m_chunks;
          size_t m_chunkIndex;
          size_t m_elementIndex;
          size_t m_increment;

        public:
           // (std::iterator_traits)
          using iterator_category = std::forward_iterator_tag;
          using value_type        = T;
          using difference_type   = std::ptrdiff_t;
          using pointer           = T*;
          using reference         = T&;

          ThreadIterator(const std::vector<Chunk*>* chunks, size_t chunkIdx, size_t elemIdx, size_t increment) : m_chunks(chunks), m_chunkIndex(chunkIdx), m_elementIndex(elemIdx), m_increment(increment) {}
          reference operator*() const {Chunk* chunk = (*m_chunks)[m_chunkIndex]; return *reinterpret_cast<T*>(&chunk->m_data[m_elementIndex * m_increment]);}

          pointer operator->() const {return &(**this);}
          
          ThreadIterator& operator++() {
            m_elementIndex++;
            // Increment chunk
            if (m_elementIndex >= (*m_chunks)[m_chunkIndex]->m_used) {
              m_chunkIndex++;
              m_elementIndex = 0;
            }
            return *this;
          }

          ThreadIterator operator++(int) {
            ThreadIterator tmp = *this;
            ++(*this); 
            return tmp;
          }

          friend bool operator==(const ThreadIterator& a, const ThreadIterator& b) {return a.m_chunks == b.m_chunks && a.m_chunkIndex == b.m_chunkIndex && a.m_elementIndex == b.m_elementIndex;}
          friend bool operator!=(const ThreadIterator& a, const ThreadIterator& b) {return !(a == b);
        }
      };

      template <size_t Thread> ThreadIterator begin() {
        auto& chunks = m_threadedChunks[Thread].m_chunks;
    
        if (chunks.empty() || chunks[0]->m_used == 0) {
          return end<Thread>();
        }
    
        return ThreadIterator(&chunks, 0, 0, m_increment);
      }

      template <size_t Thread> ThreadIterator end() {
        auto& chunks = m_threadedChunks[Thread].m_chunks;
        return ThreadIterator(&chunks, chunks.size(), 0, m_increment);
      }
    };
  

  // All blocks belonging to a single archetype.
  struct ArchetypePool {
    Signature m_signature; 
    ArchetypePool() : m_blocks(PAGEBLOCK_SIZE) {}

    ArenaAllocator<platform::PageBlock> m_blocks;
    PageblockLayoutInstance m_layout; 
    size_t m_index;

    // Adding / removing components from base
    ArenaAllocator<ArchetypePool*> m_edgePools;   
     // Index within outer m_pools;
    std::array<size_t, THREAD_COUNT> m_waterlines {0}; 
  };
}
