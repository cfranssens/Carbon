#pragma once

/* Defines what type of command the command buffer holds.
 *  SpawnEntity - Allocating entirely new data.
 *  AddComponent - Move entity data to a different buffer/stirage slot.
 *  RemoveComponent - Ditto.
 *  RemoveEntry - Adds Entity index to local freelist.
 */

#include "archetype.hpp"
#include "log.hpp"
#include <cstddef>
#include <string>

constexpr size_t COMMAND_BUFFER_SIZE = 4096;

class SpawnCommandBuffer {
public:
  // Deafult constructor to satisfy compiler for use within hash map;
  SpawnCommandBuffer() = default;
  ~SpawnCommandBuffer() = default;
  // Construct an empty command buffer.
  SpawnCommandBuffer(Archetype archetype) : m_archetype(archetype) {


  };
  template <typename... Components> size_t insert(Components &&...components) {
    // for (int i = 0; i < m_archetype.size(); i++) {
    // }

    LOG_DEBUG("Size of Entity: " + std::to_string(m_archetype.size()));
    LOG_DEBUG("Total entities in spawn buffer: " + std::to_string(COMMAND_BUFFER_SIZE / m_archetype.size()));
    return 0;
  }

private:
  Archetype m_archetype;

  alignas(64) std::byte m_storage[COMMAND_BUFFER_SIZE];

  // Component offsets, (strides). 
  std::vector<size_t> m_componentOffsets;
};
