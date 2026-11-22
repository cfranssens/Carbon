#pragma once 

#include <cstdint>
namespace world {
  // This is per world, entites cant be accessed outside their World
  struct Entity {
    uint64_t thread : 8; // 8 
    uint64_t archetype : 18; // 28
    uint64_t block : 22; // 44 
    uint64_t index : 14; // 64
  };

  static_assert(sizeof(Entity) == sizeof(uint64_t), "Entity is not aligned to word size!");
}
