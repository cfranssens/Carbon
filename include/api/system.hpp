#include "archetype.hpp"
#include "query.hpp"
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
        world::Entity entity = pool.insert<Thread, Ts...>(std::forward<Ts>(components)...);
        return entity;
      }

      void removeEntity(world::Entity& entity) {
        size_t thread = entity >> 56;
        size_t arch = (entity >> 40) & ((1ULL << 16) - 1);
        world::ArchetypePool& pool = m_storage->get(thread, arch);
  
        //std::cout << thread << ", " << arch << std::endl;
        pool.removeEntity(entity);
      }

      template <typename... Ts> world::Query<Ts...>& query() {
        world::Query<Ts...> query = world::Query<Ts...>();
        return query; 
      }

    private:
      // Function to bind storage to loose system. Avoids inheriting constructor explicitly. 
      void bind(std::shared_ptr<world::StorageArena> storage) {m_storage = storage;}
      std::shared_ptr<world::StorageArena> m_storage; 
      
      friend class Core;
  };  
}
