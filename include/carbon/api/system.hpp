#pragma once 

#include "carbon/platform/pageblock.hpp"
#include "carbon/world/archetype.hpp"
#include "carbon/world/entity.hpp"
#include "carbon/world/storage.hpp"
#include <algorithm>
#include <cstddef>
#include <array>
#include <iostream>
#include <carbon/meta/filter.hpp>

namespace api {
  template <typename World> class Core;

  // System base class to inherit from 
  template <typename World> class System {
    public: 
      System() {}
      virtual void update(float dt) = 0;
      virtual ~System() {}

    protected: 
      template<size_t Thread, typename... Ts> world::Entity createEntity(Ts&&... components) {
        using Arch = world::Archetype<Ts...>;
        //static size_t archetypeIndex = world::getLocalArchetypeIndex<Arch, World>();

        constexpr size_t BLOCK_CAPACITY = ((world::BLOCK_SIZE - (Arch::length + 1) * platform::WORD_LENGTH) << 3) / ((meta::size_sum<typename std::decay<Ts>::type...>::value << 3) + 2);

        // Fills up current block before recycling to ease tlb pressure
        // Thread template ensures each thread receives a unique block
        // Reduce static loads 
        static Cache<Thread, Arch> c = [this]{
          Cache<Thread, Arch> c;
          c.m_archetype  = world::getLocalArchetypeIndex<Arch, World>();
          c.m_allocator  = m_storage->template allocator<Arch, Thread>();
          c.m_allocator->m_capacity = BLOCK_CAPACITY;
          c.m_block      = c.m_allocator->allocate();
          c.m_blockIndex = c.m_allocator->m_currentBlock;
          return c;
        }();
        
        using Offsets = meta::canonical_offsets<BLOCK_CAPACITY, platform::WORD_LENGTH, typename Arch::canonical>;
        
        for (;;) {
          for (; c.m_block->m_waterline < (BLOCK_CAPACITY >> platform::WORD_SHIFT); c.m_block->m_waterline++) {
            size_t& word = reinterpret_cast<size_t*>(c.m_block->data())[c.m_block->m_waterline];
            if (~word == 0) continue;

            size_t shift = __builtin_ctzll(~word);
            word |= size_t(1) << shift;

            size_t index = (c.m_block->m_waterline << platform::WORD_SHIFT) | shift;
            unsigned char* base = reinterpret_cast<unsigned char*>(c.m_block->data());
            int expand[] = { 0, (storeIfData<Offsets, typename Arch::canonical>(base, index, std::forward<Ts>(components), std::is_empty<typename std::decay<Ts>::type>()), 0)... };            
            (void)expand;

            c.m_block->m_count++;

            world::Entity entity;
            entity.thread = Thread;
            entity.archetype = c.m_archetype;
            entity.block = c.m_blockIndex;
            entity.index = index;

            return entity;
          }

          refill(*c.m_allocator, c.m_block, c.m_blockIndex); 
        }
        // static size_t localArchetypeIndex = world::getLocalArchetypeIndex<Arch, World>();
      }

      void removeEntity(world::Entity entity) { 
        world::PagedAllocator* allocator = m_storage->allocator(entity.archetype * world::THREAD_COUNT + entity.thread);
        platform::PageBlock* pb = allocator->at(entity.block);
        
        size_t word = entity.index >> platform::WORD_SHIFT;
        size_t shift = entity.index & (platform::WORD_LENGTH - 1);

        reinterpret_cast<size_t*>(pb->data())[word] &= ~(size_t(1) << shift);
        
        pb->m_waterline = std::min(word, pb->m_waterline); 
        
        if (pb->m_count-- < (allocator->m_capacity / 4) * 3 && entity.block < allocator->m_cursor) {
          allocator->m_cursor = entity.block;
        }
      }

      // Queries belong to a system 
      template <typename... Ts, typename F> void query(F&& f) {
        using Query = filter::Query<Ts...>;
        static world::QuerySig<Query, World> qSig = world::QuerySig<Query, World>::get();
        qSig.debug();
        //static world::Signature sig = [];
      } 

    private: 
      template<size_t Thread, typename Arch> struct Cache {
        world::PagedAllocator* m_allocator;
        platform::PageBlock*   m_block;
        
        size_t m_blockIndex;  
        size_t m_archetype;
      };

      void refill(world::PagedAllocator& allocator, platform::PageBlock*& block, size_t& blockIndex) __attribute__((noinline, cold)) {
        while (++allocator.m_cursor <= allocator.m_currentBlock) {
          platform::PageBlock* pb = allocator.at(allocator.m_cursor);
          if (pb->m_count >= (allocator.m_capacity / 4) * 3) continue; // Skip if block is more than 75% filled

          block = allocator.at(allocator.m_cursor);
          blockIndex = allocator.m_cursor;
          return;
        } 
          
        block = allocator.allocate();
        allocator.m_cursor = allocator.m_currentBlock;
        blockIndex = allocator.m_cursor;
      }

      template <typename Offsets, typename Canon, typename T> inline void storeIfData(unsigned char*, size_t, T&&, std::true_type /*empty*/) {}

      template <typename Offsets, typename Canon, typename T> inline void storeIfData(unsigned char* base, size_t i, T&& v, std::false_type) {
        storeComponent<Offsets, Canon>(base, i, std::forward<T>(v));
      }

      template <typename Offsets, typename Canon, typename T>
        static void storeComponent(unsigned char* base, size_t index, T&& value) {
          using D = typename std::decay<T>::type;
          const size_t offset = Offsets::value[meta::index_of<D, Canon>::value];
        ::new (static_cast<void*>(base + offset + index * sizeof(D))) D(std::forward<T>(value));
      }

      // Instance bound glabal allocator 
      world::ArchetypeStorage<World>* m_storage;
      friend class api::Core<World>;
  };
}
