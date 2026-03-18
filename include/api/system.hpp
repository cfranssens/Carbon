#include "archetype.hpp"
#include "world/allocator.hpp"
#include "world/entity.hpp"
#include <cstddef>
#include <iostream>
#include <memory>

namespace api {
  // Publicly derived from base system class. 
  class System {
    public:
      System() {};
      ~System() = default; 

      virtual void update() = 0;

    protected:
      template <size_t Thread, typename... Ts> 
      world::Entity createEntity(Ts&&... components) {
        using arch = world::Archetype<Ts...>;
        static world::ArchetypePool& pool = m_storage->ensure_at<arch, Thread>();
        size_t blockIndex = pool.insert<Ts...>();

        return world::Entity {};
      }

    private:
      void bind(std::shared_ptr<world::StorageArena> storage) {m_storage = storage;}
      std::shared_ptr<world::StorageArena> m_storage; 

      friend class Core;
  };  
}
