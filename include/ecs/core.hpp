#pragma once

#include "ecs/archetype.hpp"
#include "ecs/command_buffer.hpp"

#include <ankerl/unordered_dense.h>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <utility>

constexpr size_t RANGE_FREE_SPACE = 1;

struct RangeEntry {
  size_t m_begin, m_end, m_index;
};

class Core {
public:
  Core() = default;

  // move a storage buffer into the map
  void insertStorageBuffer(StorageBuffer &&storageBuffer) {
    ArchetypeMask mask = storageBuffer.archetype().mask();
    auto it = m_archetypeRanges.find(mask);
    // Not found
    if (it == m_archetypeRanges.end()) {
      // Create a new empty range at the end.
      size_t end = m_storage.size();
      m_archetypeRanges[mask] = {end, end + RANGE_FREE_SPACE, end};
      m_storage.resize(end + RANGE_FREE_SPACE);

      m_storage[end] = storageBuffer;
    } else {
      RangeEntry &range = m_archetypeRanges[mask];
      range.m_end++;

      m_storage.resize(m_storage.size() + 1);

      ++it;
      for (; it != m_archetypeRanges.end(); it++) {
        std::swap(storageBuffer, m_storage[it->second.m_begin]);
        it->second.m_begin++;
        it->second.m_end++;
        it->second.m_index++;
      }

      m_storage[m_storage.size() - 1] = storageBuffer;
    }
  };

  void debugRanges() {
    std::cout << "Buffers: " << m_storage.size() << std::endl;
    int i = 0;
    for (auto &[key, range] : m_archetypeRanges) {
      std::cout << "Range " << i++ << ", {" << range.m_begin << " : "
                << range.m_end << "}" << std::endl;

      for (int j = 0; j < range.m_end - range.m_begin; j++) {
        std::cout << std::bitset<16>(m_storage[range.m_begin + j]
                                         .archetype()
                                         .mask()
                                         .m_scalarMask[0])
                  << std::endl;
      }
    }
  }

private:
  // Storage buffer ranges, contigious in memory.
  // This acts as a collection of ranges that gets sorted on insert.

  std::vector<StorageBuffer> m_storage;
  // A range is start, end and current index.
  // Because insertion order interation is needed for rebuilding this is
  // unordered_dense.
  ankerl::unordered_dense::map<ArchetypeMask, RangeEntry, ArchetypeHash>
      m_archetypeRanges;
};
