#pragma once 

#include "carbon/world/archetype.hpp"
#include <carbon/platform/allocator.hpp>

namespace world {
  using PagedAllocator = platform::ArenaAllocator<32 * 1024>;
  
  // Storage object belonging to a single archetype 
  class ArchetypeStorage {
    public:
      ArchetypeStorage() = default;
    
      template <typename Arch> void createStorage() {
        static size_t archetypeIndex = world::getLocalArchetypeIndex<Arch>();
        std::cout << archetypeIndex << std::endl;
      }

    private: 
      PagedAllocator m_storages;
  };
}
