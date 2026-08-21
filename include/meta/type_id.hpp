#pragma once 

// Metaprogramming for ordering
#include "meta/type_list.hpp"
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <numeric>
#include <type_traits>

namespace meta {
  struct TypeInfo {
    size_t index; 
    size_t size; 
    size_t align;
    bool isConst;
    
    bool triviallyCopyable;

   void (*copyConstruct)(void* dst, const void* src) = nullptr;
    void (*destroy)(void* ptr) = nullptr;
  };

  // Freestanding type counter that can be used globally. 
  template <size_t World> inline std::atomic<size_t> worldTypeCounter {0};

  template <size_t World, typename T> struct TypeID {
    using CleanT = std::remove_cvref_t<T>;

    inline static const size_t index = worldTypeCounter<World>.fetch_add(1, std::memory_order_relaxed);
    inline static constexpr size_t size = sizeof(CleanT);
    inline static constexpr size_t align = alignof(CleanT);
    inline static constexpr bool isConst = std::is_const_v<T>;
    inline static constexpr bool triviallyCopyable = std::is_trivially_copyable_v<CleanT>; 

    static void copyConstructImpl(void* dst, const void* src) {
      new (dst) CleanT(*reinterpret_cast<const CleanT*>(src));
    }
    static void destroyImpl(void* ptr) {
      reinterpret_cast<CleanT*>(ptr)->~CleanT();
    }

    inline static TypeInfo instance() {
      if constexpr (triviallyCopyable) {
        return TypeInfo {index, size, align, isConst, triviallyCopyable, nullptr, nullptr};
      } else {
        return TypeInfo {index, size, align, isConst, triviallyCopyable, &copyConstructImpl, &destroyImpl};
      }
    }
  };

  template <size_t World, typename List> inline const auto typeIndexPermutation = [] {}; 
  template <size_t World, typename... Ts> inline const auto typeIndexPermutation<World, type_list<Ts...>> = [] {
    constexpr size_t N = sizeof...(Ts);
        
    std::array<size_t, N> indices{};
    std::iota(indices.begin(), indices.end(), 0);
      std::array<size_t, N> ids = { meta::TypeID<World, Ts>().index... };

      std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return ids[a] < ids[b];
      });

      return indices;
  }();
}
