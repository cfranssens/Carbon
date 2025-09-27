#pragma once

// Contians info regarding the archetype.
#include "ecs/component.hpp"
#include "log.hpp"
#include <algorithm>
#include <cstddef>
#include <numeric>
#include <string>
#include <vector>

#include <simde/x86/avx2.h>

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
    std::iota(archetype.m_order.begin(), archetype.m_order.end(), 0);

    (
        [&] {
          // Clean type without references / ptrs.
          using CleanT = std::remove_cvref_t<Components>;
          size_t index = ComponentID::get<CleanT>();
          // componentIds.emplace_back(index);
          archetype.m_size += sizeof(CleanT);
        }(),
        ...);

    // Component ids should now hold all the collected component ids to be
    // sorted.

    // std::sort()

    return archetype;
  }

  size_t size() { return m_size; }

private:
  // Order in which components should be assigned.
  std::vector<size_t> m_order;
  size_t m_size;
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
