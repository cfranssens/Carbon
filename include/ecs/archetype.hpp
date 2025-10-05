#pragma once

// Contians info regarding the archetype.
#include "ecs/component.hpp"
#include "log.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

#include <simde/x86/avx2.h>

union ArchetypeMask {
  simde__m256i m_mask;
  uint64_t m_scalarMask[4];

  ArchetypeMask() { std::memset(this, 0, sizeof(ArchetypeMask)); };

  void merge(ArchetypeMask rhs) {
    m_mask = simde_mm256_or_si256(m_mask, rhs.m_mask);
  };
};

inline bool operator==(const ArchetypeMask &a,
                       const ArchetypeMask &b) noexcept {
  return std::memcmp(&a, &b, sizeof(ArchetypeMask)) == 0;
}

struct ArchetypeHash {
  size_t operator()(const ArchetypeMask &k) const noexcept {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (int i = 0; i < 4; ++i) {
      h ^= k.m_scalarMask[i];
      h *= 0x100000001b3ULL;
    }
    return static_cast<size_t>(h);
  }
};

// Intermediate type that holds component sets and info.
// Includes information to sort corresponding component data.
class Archetype {
public:
  // Needed because c++ doesnt allow templated arguments by type within a
  // constructor.
  template <typename... Components> static Archetype fromComponents() {
    Archetype archetype;
    size_t packLength = sizeof...(Components);
    // The underlying component ids used to build a stable order that is
    // consistent across archetype definitions.
    std::vector<size_t> componentIds;

    componentIds.reserve(packLength);
    // Resize to avoid UB.
    archetype.m_order.resize(packLength);
    archetype.m_sizes.resize(packLength);

    std::iota(archetype.m_order.begin(), archetype.m_order.end(), 0);

    (
        [&] {
          // Clean type without references / ptrs.
          using CleanT = std::remove_cvref_t<Components>;
          size_t index = ComponentID::get<CleanT>();
          // componentIds.emplace_back(index);
          archetype.m_sizes[archetype.m_count++] = sizeof(CleanT);
          archetype.m_size += sizeof(CleanT);

          int scalarIndex = index >> 6;
          int bitShift = index & 63;

          archetype.m_mask.m_scalarMask[scalarIndex] |= (1ULL << bitShift);
        }(),
        ...);

    // Component ids should now hold all the collected component ids to be
    // sorted.

    // std::sort()

    return archetype;
  }

  inline size_t size() noexcept { return m_size; }
  inline size_t count() noexcept { return m_count; }

  inline size_t *componentSizes() noexcept { return m_sizes.data(); }
  inline size_t *componentOrder() noexcept { return m_order.data(); }

  ArchetypeMask mask() { return m_mask; };

private:
  // Order in which components should be assigned.
  std::vector<size_t> m_order;
  std::vector<size_t> m_sizes;
  size_t m_size = 0;
  size_t m_count = 0;

  ArchetypeMask m_mask;
};

// Signature class to define component layout within a buffer or storage for
// batched operation across threads.
class ArchetypeSignature {
public:
  ArchetypeSignature() = default;

private:
  // Defines what component types from the global registry are present.
  simde__m256i m_mask;
  // Size of all components in bytes.
  size_t m_size;
  // Size per component in natural order.
  std::vector<size_t> m_componentSizes;
};
