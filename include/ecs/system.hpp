#pragma once

#include "ecs/archetype.hpp"
#include "ecs/command_buffer.hpp"
#include "ecs/component.hpp"
#include "ecs/core.hpp"
#include "log.hpp"
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <flat_hash_map.hpp>
#include <type_traits>
#include <utility>
#include <vector>

// Base class to inherit from.
class System {
public:
  // Actual system job
  virtual void update() = 0;

protected:
  // Create an entity
  template <typename... Components>
  EntityID createEntity(Components &&...components) {
    uint64_t hash = fastComponentHash<Components...>();

    if (m_recipes.find(hash) == m_recipes.end()) {
      // Hash does not exist yet (archetype either isn't defined yet or not
      // present in this component order).
      Archetype archetype = Archetype::fromComponents<Components...>();
      // Define buffer layout up front
      m_recipes[hash] = {archetype};

      m_writeMask.merge(archetype.mask());

      LOG_DEBUG("Total mask: " +
                std::bitset<64>(m_writeMask.m_scalarMask[0]).to_string());
    }

    EntityID id;
    id.m_bufferIndex = m_recipes[hash].insert(std::move(components)...);
    return id;
  }

  // DEBUG
  void flushAllStorageBuffers(Core *core) {
    for (auto &[key, buffer] : m_recipes) {
      Archetype arch = buffer.archetype();
      core->insertStorageBuffer(std::move(buffer));

      // Reconmstruct what? There's nothing left.
      buffer = StorageBuffer(arch);
    };
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
  ska::flat_hash_map<uint64_t, StorageBuffer> m_recipes;

  // Mask of all components accessed by the system to resolve confilcts,
  ArchetypeMask m_readMask;
  ArchetypeMask m_writeMask;
};

union SystemID {
  uint64_t packed;
  struct {
    uint32_t layer, id;
  };
};

struct Layer {
  std::vector<SystemID> m_systems;
};

// Collects all systems and manages workers
class SystemScheduler {
public:
  SystemScheduler() = default;

  template <typename T> void registerSystem() {
    static_assert(std::is_base_of_v<System, T>, "T must derive from System.");
    SystemID id;
    id.id = m_systems.size();

    // What the helly.
    m_systems.emplace(id.packed,
                      std::unique_ptr<System>(std::make_unique<T>().release()));
  }

  // Quick debug call to update all systems
  void update() {
    for (auto &[id, system] : m_systems) {
      system->update();
    }
  }

private:
  ska::flat_hash_map<uint64_t, std::unique_ptr<System>> m_systems;
  std::vector<Layer> m_layers;
};
