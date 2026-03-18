#pragma once 

#include "build.hpp"
#include "api/system.hpp"
#include "world/allocator.hpp"
#include <memory>
#include <type_traits>

namespace api {
  // Core entry point
  class Core {
    public: 
      Core() : m_storage(std::make_shared<world::StorageArena>()) {}
      ~Core() {};

      template <typename S> 
      void registerSystem() {
        static_assert(std::is_base_of_v<api::System, S>, "System is not derived from System Base");
        std::unique_ptr<System> sys = std::make_unique<S>();

        sys->bind(m_storage);
        sys->update();
      } 

    private: 
      std::shared_ptr<world::StorageArena> m_storage;
  };
}
