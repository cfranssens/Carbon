#pragma once

#include "ecs/archetype.hpp"
#include "ecs/command_buffer.hpp"
#include "ecs/component.hpp"
#include "log.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

#include <flat_hash_map.hpp>
#include <utility>

using Recipe = size_t;

// Base class to inherit from.
class System {
public:
  template <typename... Components> void test() {
    // LOG_DEBUG(std::to_string(fastComponentHash<Components...>()));

    createEntity(1.0f, 1ULL);
  };

protected:
  // Create an entity
  template <typename... Components>
  size_t createEntity(Components &&...components) {
    uint64_t hash = fastComponentHash<Components...>();

    if (m_recipes.find(hash) == m_recipes.end()) {
      // Hash does not exist yet (archetype either isn't defined yet or not
      // present in this component order).
      Archetype archetype = Archetype::fromComponents<Components...>();
      // Define buffer layout up front
      m_recipes[hash] = {archetype};
    }

    return m_recipes[hash].insert(std::move(components)...);
  }

private:
  // Order dependent hash
  template <typename... Components> static uint64_t fastComponentHash() {
    uint64_t hash = 0;
    const uint64_t ids[]{ComponentID::get<Components>()...};
    for (int i = 0; i < sizeof...(Components); ++i) {
      hash ^= ids[i] << (i * 8);
    }
    return hash;
  }

  // Store recipes for constructor calls.
  ska::flat_hash_map<uint64_t, SpawnCommandBuffer> m_recipes;
};
