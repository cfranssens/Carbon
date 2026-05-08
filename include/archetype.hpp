#pragma once

#include "meta/type_id.hpp"
#include "meta/type_list.hpp"
#include <bitset>
#include <cstdint>

namespace world {
    // Runtime representation of a set number of unique component types. 
    struct Signature {
        uint64_t m_bitset = 0; // To be dynamically expanded if exceeded.

        // Testwhether rhs completely overlaps. 
        inline bool matches(const Signature& rhs) const {
            return (m_bitset & rhs.m_bitset) == rhs.m_bitset; // rhs completely overlaps 
        }

        // SEt a single bit. 
        inline void set(int n) {
            m_bitset |= 1ull << n; 
        }
        inline void clear(int n) {
            m_bitset &= ~(1ull << n);
        }

        inline bool get(int n) {
            return m_bitset & (1ull << n);
        }
    } ;

    // Constexpr representation of a canonicalised collection of types. 
    template <typename... Ts>
    struct Archetype {
        using canonical_types = typename meta::sort_types<meta::type_list<Ts...>>::type;
        
        static constexpr size_t size = (sizeof(Ts) + ...);
        static constexpr size_t length = sizeof...(Ts);
     
        static const inline Signature signature = [](){
            Signature sig;
            (sig.set(meta::type_index<Ts>()), ...);
            return sig;
        }(); 
    };
}

#ifndef NDEBUG
template<>
struct std::formatter<world::Signature, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const world::Signature& sig, FormatContext& ctx) const {
        auto out = ctx.out();
        *out++ = std::bitset<64>(sig.m_bitset);
        return out;
    }
};
#endif 
