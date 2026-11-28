#pragma once 

#include "carbon/platform/allocator.hpp"

#include <atomic>
#include <bitset>
#include <carbon/meta/type_list.hpp>
#include <cstddef>

namespace world {
  constexpr size_t ceil_div(size_t a, size_t b) { return (a + b - 1) / b; }

  #define MAX_TYPES 128
  static constexpr size_t WORDS = ceil_div(MAX_TYPES, platform::WORD_LENGTH);

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
  }

  template<typename World> inline std::atomic<size_t> worldTypeCounter {0};
  // Gets Type index local to N
  template <typename T, typename World> size_t getLocalTypeIndex() {
    static size_t index = world::worldTypeCounter<World>.fetch_add(1, std::memory_order_relaxed);
    return index;
  }

  constexpr size_t max3(size_t a, size_t b, size_t c) {
    return a > b ? (a > c ? a : c) : (b > c ? b : c);
  }
  template <size_t WORDS, typename World> struct SetBits {
    size_t (&words)[WORDS];

    template <typename T, size_t I> void operator()() const {
      using D = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
      const size_t id = getLocalTypeIndex<D, World>();
    
      words[id >> platform::WORD_SHIFT] |= size_t(1) << (id & (platform::WORD_LENGTH - 1));
    }
  };

  template <typename Query, typename World> struct QuerySig {
    size_t m_queries[WORDS];
    size_t m_withs[WORDS];
    size_t m_withouts[WORDS];

    static const QuerySig& get() {
      static const QuerySig sig = build();
      return sig;
    }

    void debug() {
      std::cout << "QUERY: ";
      for (size_t i = 0; i < WORDS; i++) std::cout << std::bitset<platform::WORD_LENGTH>(m_queries[WORDS - 1 - i]) << " ";
      std::cout << std::endl << "WITH:  ";
      for (size_t i = 0; i < WORDS; i++) std::cout << std::bitset<platform::WORD_LENGTH>(m_withs[WORDS - 1 - i]) << " ";
      std::cout << std::endl << "WOUTS: ";
      for (size_t i = 0; i < WORDS; i++) std::cout << std::bitset<platform::WORD_LENGTH>(m_withouts[WORDS - 1 - i]) << " ";
      std::cout << std::endl;
    }

    private:
      template <typename List> static void fill(size_t (&words)[WORDS]) {
      meta::for_each_type<List>::apply(SetBits<WORDS, World>{words});
    }

    static QuerySig build() {
      QuerySig s = QuerySig(); 
      fill<typename Query::queries>(s.m_queries);
      fill<typename Query::withs>(s.m_withs);
      fill<typename Query::withouts>(s.m_withouts);
      return s;
    }
  };
}
