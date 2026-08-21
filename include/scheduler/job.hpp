#pragma once 

#include "api/core.hpp"
#include "platform/pageblock.hpp"
#include <world/entity.hpp>
#include <array>
#include <cstddef>
#include <cstdint>

#include <tuple>
namespace scheduler {
  template <typename Fn, typename... Ts> struct QueryJobPayload {
    platform::PageBlock* m_block;
    std::array<size_t, sizeof...(Ts)> m_offsets;
    Fn* m_userFn; 
  
    // For reconstructing world::Entity
    uint64_t m_baseEntityID;
  };

template <typename Payload, typename Fn, typename... Ts, size_t... Is>
static inline void executeBlock(Payload* payload, std::index_sequence<Is...>) {
    platform::PageBlock* block = payload->m_block;
    Fn& fn = *(payload->m_userFn);
    uint64_t baseEntityID = payload->m_baseEntityID;

    std::tuple<Ts*...> arrays = std::make_tuple(
      reinterpret_cast<Ts*>(__builtin_assume_aligned(reinterpret_cast<char*>(block->data()) + payload->m_offsets[Is], 64))...
    );

    auto runVectorized = [&](uint64_t start, uint64_t length, auto* __restrict__ ... ptrs) {
    if constexpr (std::is_invocable_v<Fn, world::Entity, decltype(*ptrs)...>) {
          #pragma GCC ivdep 
          #pragma clang loop vectorize(assume_safety) interleave(enable)
          for (uint64_t i = 0; i < length; ++i) {
              uint64_t entityIndex = start + i;
              
              world::Entity e = baseEntityID | (entityIndex & 0xFFFFFFULL);
              
              fn(e, ptrs[entityIndex]...);
          }
          
          
      } else {  
          #pragma GCC ivdep 
          #pragma clang loop vectorize(assume_safety) interleave(enable)
          for (uint64_t i = 0; i < length; ++i) {
              fn(ptrs[start + i]...);
          }
          
      }
    };

    uint64_t activeStart = 0;
    uint64_t activeLength = 0;


    for (uint64_t wordIndex = 0; wordIndex < ((block->m_count + 63) >> 6); ++wordIndex) {
      uint64_t word = reinterpret_cast<uint64_t*>(block->data())[wordIndex]; 
      
      while (word != 0) {
        uint64_t lsb = word & -word;
        uint64_t next = word + lsb;
        uint64_t bitBlock = word & ~next;

        int start = std::countr_zero(bitBlock); 
        int length = std::popcount(bitBlock);
        
        uint64_t absoluteStart = (wordIndex * 64) + start;
        if (activeLength > 0 && (activeStart + activeLength == absoluteStart)) {
            activeLength += length; 
        } else {
          if (activeLength > 0) {
            runVectorized(activeStart, activeLength, std::get<Is>(arrays)...);
          }
            
          activeStart = absoluteStart;
          activeLength = length;
        }

        word ^= bitBlock;
      }
    }

    if (activeLength > 0) {
      runVectorized(activeStart, activeLength, std::get<Is>(arrays)...);
    }
}
}
