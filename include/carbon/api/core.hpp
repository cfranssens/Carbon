#pragma once 

#include "carbon/api/system.hpp"
#include "carbon/world/storage.hpp"
#include <memory>

namespace api {
  // Instance data
  template <typename World> class Core {
    public: 
      Core() {}
      ~Core() {}
    
      template <typename Sys> void registerSystem() {
        static Sys s; 
        System<World>* sys = &s;
        sys->m_storage = &m_storage; 
        sys->update(0.0f);
        return;
      }

    private: 
      world::ArchetypeStorage<World> m_storage;
  };
}
