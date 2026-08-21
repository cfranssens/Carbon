#pragma once 

#include "meta/type_id.hpp"
#include "meta/type_list.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>
#include <simde/x86/avx512.h>
#include <vector>

namespace world {
  namespace util {
    constexpr uint64_t align_up(uint64_t val, uint64_t alignment) {
      return (val + alignment - 1) & ~(alignment - 1);
    }
  }


  #if ECS_MAX_COMPONENTS <= 64
    using simd_vector_t = uint64_t;
    inline constexpr size_t SIMD_WORDS = 1;
    inline constexpr size_t SIMD_ALIGN = 8;
  #elif ECS_MAX_COMPONENTS <= 128
    using simd_vector_t = simde__m128i;
    inline constexpr size_t SIMD_WORDS = 2;
    inline constexpr size_t SIMD_ALIGN = 16;
  #elif ECS_MAX_COMPONENTS <= 256
    using simd_vector_t = simde__m256i;
    inline constexpr size_t SIMD_WORDS = 4;
    inline constexpr size_t SIMD_ALIGN = 32;
  #elif ECS_MAX_COMPONENTS <= 512
    using simd_vector_t = simde__m512i;
    inline constexpr size_t SIMD_WORDS = 8;
    inline constexpr size_t SIMD_ALIGN = 64;
  #else
    #error "ECS_MAX_COMPONENTS must be 512 or less."
  #endif

namespace simd_utils {
    // 64-bit
    inline bool test_all(uint64_t a, uint64_t b) { return (a & b) == b; }
    inline bool test_none(uint64_t a, uint64_t b) { return (a & b) == 0; }
    inline uint64_t bitwise_or(uint64_t a, uint64_t b) { return a | b; }
    inline uint64_t bitwise_and(uint64_t a, uint64_t b) { return a & b; }
    inline uint64_t bitwise_xor(uint64_t a, uint64_t b) { return a ^ b; }
    inline size_t popcount(uint64_t a) { return std::popcount(a); }
    inline bool equal(uint64_t a, uint64_t b) {  return a == b; }

    // 128-bit (SSE2)
    inline bool test_all(simde__m128i a, simde__m128i b) { return simde_mm_testc_si128(a, b); }
    inline bool test_none(simde__m128i a, simde__m128i b) { return simde_mm_testz_si128(a, b); }
    inline simde__m128i bitwise_or(simde__m128i a, simde__m128i b) { return simde_mm_or_si128(a, b); }
    inline simde__m128i bitwise_and(simde__m128i a, simde__m128i b) { return simde_mm_and_si128(a, b); }
    inline simde__m128i bitwise_xor(simde__m128i a, simde__m128i b) { return simde_mm_xor_si128(a, b); }
    inline bool equal(simde__m128i a, simde__m128i b) { 
        simde__m128i diff = bitwise_xor(a, b);
        return simde_mm_testz_si128(diff, diff); 
    }

    // 256-bit (AVX2)
    inline bool test_all(simde__m256i a, simde__m256i b) { return simde_mm256_testc_si256(a, b); }
    inline bool test_none(simde__m256i a, simde__m256i b) { return simde_mm256_testz_si256(a, b); }
    inline simde__m256i bitwise_or(simde__m256i a, simde__m256i b) { return simde_mm256_or_si256(a, b); }
    inline simde__m256i bitwise_and(simde__m256i a, simde__m256i b) { return simde_mm256_and_si256(a, b); }
    inline simde__m256i bitwise_xor(simde__m256i a, simde__m256i b) { return simde_mm256_xor_si256(a, b); }
    inline bool equal(simde__m256i a, simde__m256i b) { 
        simde__m256i diff = bitwise_xor(a, b);
        return simde_mm256_testz_si256(diff, diff); 
    }

    // 512-bit (AVX-512)
    inline bool test_all(simde__m512i a, simde__m512i b) { 
        return simde_mm512_test_epi64_mask(a, b) == simde_mm512_test_epi64_mask(b, b); 
    }
    inline bool test_none(simde__m512i a, simde__m512i b) { 
        return simde_mm512_test_epi64_mask(a, b) == 0; 
    }
    inline simde__m512i bitwise_or(simde__m512i a, simde__m512i b) { return simde_mm512_or_si512(a, b); }  
    inline simde__m512i bitwise_and(simde__m512i a, simde__m512i b) { return simde_mm512_and_si512(a, b); }  
    inline simde__m512i bitwise_xor(simde__m512i a, simde__m512i b) { return simde_mm512_xor_si512(a, b); }
    inline bool equal(simde__m512i a, simde__m512i b) { 
        simde__m512i diff = bitwise_xor(a, b);
        return simde_mm512_test_epi64_mask(diff, diff) == 0; 
    }
}

struct alignas(SIMD_ALIGN) Signature {
    union {
        simd_vector_t m_simd;
        uint64_t m_words[SIMD_WORDS];
    };

    inline void set(size_t index) {
        m_words[index >> 6] |= (1ULL << (index & 63));
    }
    inline void clear(size_t index) {
        m_words[index >> 6] &= ~(1ULL << (index & 63));
    }
    inline bool test(size_t index) const {
        return (m_words[index >> 6] >> (index & 63)) & 1ULL;
    }

    size_t popcount() const {
        size_t total = 0;
        for (size_t i = 0; i < SIMD_WORDS; ++i) {
            total += std::popcount(m_words[i]); 
        }
        return total;
    }
    size_t popcount(size_t bound) const {
      const size_t maxBits = SIMD_WORDS * 64;
    
      if (bound >= maxBits) {
        return popcount(); 
      }

      size_t total = 0;
      size_t wordIdx = bound >> 6;   
      size_t bitOffset = bound & 63; 

      for (size_t i = 0; i < wordIdx; ++i) {
        total += std::popcount(m_words[i]);
      }

      if (bitOffset > 0) {
        uint64_t mask = (1ULL << bitOffset) - 1; 
        total += std::popcount(m_words[wordIdx] & mask);
      }

      return total;
    }

    size_t ctz() const {
        size_t zeros = 0;
        for (size_t i = 0; i < SIMD_WORDS; ++i) {
            uint64_t word = m_words[i];
            if (word != 0) {
                return zeros + std::countr_zero(word); 
            }
            zeros += 64;
        }
        return zeros; 
    }
    // For query resolution
    static inline bool match(const Signature& archetype, const Signature& with, const Signature& without) {
        bool withMatch = simd_utils::test_all(archetype.m_simd, with.m_simd);
        bool withoutMatch = simd_utils::test_none(archetype.m_simd, without.m_simd);
        return withMatch && withoutMatch;
    }
    
    Signature operator|(const Signature& rhs) const {
        return Signature(simd_utils::bitwise_or(m_simd, rhs.m_simd));
    }

    Signature operator&(const Signature& rhs) const {
        return Signature(simd_utils::bitwise_and(m_simd, rhs.m_simd));
    }

    Signature& operator|=(const Signature& rhs) {
        m_simd = simd_utils::bitwise_or(m_simd, rhs.m_simd);
        return *this;
    }

    Signature& operator&=(const Signature& rhs) {
        m_simd = simd_utils::bitwise_and(m_simd, rhs.m_simd);
        return *this;
    }

    bool operator==(const Signature& rhs) const {
        return simd_utils::equal(m_simd, rhs.m_simd);
    }

    bool operator!=(const Signature& rhs) const {
        return !(*this == rhs);
    }


    Signature() {
        std::memset(this, 0, sizeof(Signature));
    }

    Signature(simd_vector_t simd) {
        m_simd = simd;
    }
  };  // Constexpr representation of a canonicalised collection of types. 
  
  // Untyped version
  struct PageblockLayoutInstance {
    size_t size;
    size_t length;
    uint64_t capacity;
    uint64_t bitsetRegion;

    std::vector<uint64_t> componentRegions;
    // Declared order. 
    std::vector<meta::TypeInfo> typeInfos;

    PageblockLayoutInstance() {}

    // Recalculate from runtime list
PageblockLayoutInstance(std::vector<meta::TypeInfo> inputInfos) : typeInfos(inputInfos), length(inputInfos.size()) {  
    std::sort(this->typeInfos.begin(), this->typeInfos.end(), [](const auto& a, const auto& b) {
        return a.index < b.index;
    });

    size_t maxT = 0;
    size = 0;
    for (auto& t : this->typeInfos) {
        if (t.size > maxT) maxT = t.size; 
        size += t.size;
    }

    capacity = ((PAGEBLOCK_SIZE << 3) - (length * (64 - maxT))) / ((size << 3) + 2);
    bitsetRegion = world::util::align_up(capacity >> 3, 64); 

    std::vector<uint64_t> offsets(length);
    uint64_t currentOffset = bitsetRegion;

    for (size_t i = 0; i < length; ++i) {
        currentOffset = world::util::align_up(currentOffset, 64);
        offsets[i] = currentOffset;
        currentOffset += capacity * this->typeInfos[i].size;
    }

    componentRegions = std::move(offsets);
    }
  };


  template <typename... Ts> struct Archetype {
    using canonical_types = typename meta::sort_types<meta::type_list<Ts...>>::type;
        
    static constexpr size_t size = (sizeof(Ts) + ...);
    static constexpr std::array<uint64_t, sizeof...(Ts)> sizes = { sizeof(Ts)... };
    static constexpr size_t length = sizeof...(Ts);

    // Layout for pageblocks, (relative offsets) 
    // PB_SIZE is in bits!!
    // Capacity = PB_SIZE - [ L * (64 - M) ] / (8 * S + 1)  
   
    static constexpr uint64_t capacity = ((PAGEBLOCK_SIZE << 3) - (length * (64 - std::max({sizeof(Ts)...})))) / ((size << 3) + 2);

    static constexpr uint64_t bitsetRegion = world::util::align_up(capacity >> 3, 64); // 8 bits per byte 

    template<size_t World> static inline const auto componentRegions = []() {
      std::array<uint64_t, sizeof...(Ts)> offsets{};
      std::array<size_t, sizeof...(Ts)> indices = meta::typeIndexPermutation<World, meta::type_list<Ts...>>;
    
      uint64_t currentOffset = bitsetRegion;
        for (size_t idx : indices) {
          currentOffset = world::util::align_up(currentOffset, 64);
          offsets[idx] = currentOffset;
          currentOffset += capacity * sizes[idx];
      }
    
      return offsets;
    }();

    // Fast
    template <size_t World> static PageblockLayoutInstance layoutInstance() {
        std::vector<meta::TypeInfo> infos;
        (infos.emplace_back(meta::TypeID<World, Ts>::instance()), ...);

        PageblockLayoutInstance instance(std::move(infos));
        //std::cout << "Capacity: " << capacity << std::endl;

        return instance;
    }
  };
}

namespace std {
    template <>
    struct hash<world::Signature> {
        size_t operator()(const world::Signature& sig) const noexcept {
            size_t hash_value = 0;
            for (size_t i = 0; i < world::SIMD_WORDS; ++i) {
                hash_value ^= std::hash<uint64_t>()(sig.m_words[i]) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
            }
            
            return hash_value;
        }
    };
}
