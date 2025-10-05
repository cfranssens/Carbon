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
#include <cstdint>
#include <cstring>
#include <string>

constexpr size_t COMMAND_BUFFER_SIZE = 4096;
union EntityID {
  uint64_t m_packed;
  struct {
    uint32_t m_bufferID;
    uint32_t m_bufferIndex;
  };
}; // Buffer ID + Local Index

// These are different from command buffers as they are directly moved into
// memory.
class StorageBuffer {
public:
  // Deafult constructor to satisfy compiler for use within hash map;
  StorageBuffer() = default;
  ~StorageBuffer() = default;
  // Construct an empty command buffer.
  StorageBuffer(Archetype archetype) : m_archetype(archetype) {
    // At this point Components and m_archetype will match.
    m_capacity = COMMAND_BUFFER_SIZE / m_archetype.size();
    m_componentOffsets.resize(m_archetype.count());

    for (int k = 0; k < m_archetype.count() - 1; k++) {
      size_t size =
          m_archetype.componentSizes()[m_archetype.componentOrder()[k]];
      m_componentOffsets[k + 1] = size * m_capacity;
      LOG_DEBUG("Stride " + std::to_string(size * m_capacity));
    }

    LOG_DEBUG("Size of Entity: " + std::to_string(m_archetype.size()));
    LOG_DEBUG("Total entities in spawn buffer: " + std::to_string(m_capacity));
  };

  template <typename... Components> size_t insert(Components &&...components) {
    if (m_waterline >= m_capacity) {
      return m_capacity;
    } // Is full submit.

    size_t freeIndex = m_waterline; // This is guearanteed to be free
    if (!m_freelist.empty()) {
      freeIndex = m_freelist.back();
    } else {
      m_waterline++;
    }

    int k = 0;
    (
        [&] {
          size_t orderedIndex = m_archetype.componentOrder()[k++];
          size_t offset = m_componentOffsets[orderedIndex];
          size_t size = m_archetype.componentSizes()[orderedIndex];

          size_t byteAddress = offset + (size * freeIndex);
          LOG_DEBUG(
              "Index: " + std::to_string(freeIndex) +
              ", Written to byte address: " + std::to_string(byteAddress));

          // Move components into storage.
          std::memmove(&m_storage[byteAddress], &components, size);
        }(),
        ...);

    return freeIndex;
  }

  inline Archetype archetype() noexcept { return m_archetype; }

private:
  Archetype m_archetype;

  // Amount of entities that can be stored according to the archetype.
  size_t m_capacity;

  alignas(64) std::byte m_storage[COMMAND_BUFFER_SIZE];

  // Component offsets, (strides).
  std::vector<size_t> m_componentOffsets;

  std::vector<size_t> m_freelist;
  size_t m_waterline = 0;
};
