#pragma once 

#include <atomic>
#include <carbon/meta/type_list.hpp>

namespace world {
  template <typename L> struct ArchetypeImpl;
  template <typename... Ts> struct ArchetypeImpl<meta::type_list<Ts...>>{
    using canonical = meta::type_list<Ts...>;
    static constexpr size_t length = sizeof...(Ts);
  //static constexpr size_t capacity = (platform::WORD_MAX - (length * std::max({sizeof(Ts)...}))) / (((sizeof(Ts) + ...) << 3) + 2);
  };

  template <typename... Ts>
  using Archetype = ArchetypeImpl<typename meta::sort_types<meta::type_list<Ts...>>::type>;

  template<typename World> inline std::atomic<size_t> worldArchetypeCounter {0};
  // Gets archetype index local to N
  template <typename Arch, typename World> size_t getLocalArchetypeIndex() {
    static size_t index = world::worldArchetypeCounter<World>.fetch_add(1, std::memory_order_relaxed);
    return index;
  }}
