#pragma once 

#include "carbon/platform/pageblock.hpp"
#include "carbon/world/archetype.hpp"
#include "carbon/world/entity.hpp"
#include "carbon/world/storage.hpp"
#include <cstddef>
#include <array>
#include <iostream>


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
        static size_t archetypeIndex = world::getLocalArchetypeIndex<Arch, World>();

        constexpr size_t BLOCK_CAPACITY =
          ((world::BLOCK_SIZE - (Arch::length + 1) * platform::WORD_LENGTH) << 3) / ((meta::size_sum<typename std::decay<Ts>::type...>::value << 3) + 2);

        // Stable pointer
        static world::PagedAllocator* allocator = m_storage->template allocator<Arch, Thread>();
        // Fills up current block before recycling to ease tlb pressure
        // Thread template ensures each thread receives a unique block
        static platform::PageBlock* currentBlock = allocator->allocate();

        using Offsets = meta::canonical_offsets<BLOCK_CAPACITY, platform::WORD_LENGTH, typename Arch::canonical>;
        
        for (;;) {
          for (; currentBlock->m_waterline < (BLOCK_CAPACITY >> platform::WORD_SHIFT); currentBlock->m_waterline++) {
            size_t& word = reinterpret_cast<size_t*>(currentBlock->data())[currentBlock->m_waterline];
            if (~word == 0) continue;

            size_t shift = __builtin_ctzll(~word);
            word |= size_t(1) << shift;

            size_t index = (currentBlock->m_waterline << platform::WORD_SHIFT) | shift;
            unsigned char* base = reinterpret_cast<unsigned char*>(currentBlock->data());
            int expand[] = { 0, (storeComponent<Offsets, typename Arch::canonical>(base, index, std::forward<Ts>(components)), 0)... }; 
            (void)expand;

            world::Entity entity;
            entity.archetype = archetypeIndex;
            entity.block = allocator->m_currentBlock;
            entity.index = index;
            return entity;
          }
   
          // TODO! Find first block with empty space that matches the thread, or is unclaimed.
          currentBlock = allocator->allocate();

          // static size_t localArchetypeIndex = world::getLocalArchetypeIndex<Arch, World>();
        }
      }

    private: 
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
