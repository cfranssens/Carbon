#pragma once

#include "meta/type_list.hpp"
#include <atomic>
#include <span>
#include <vector>

namespace world {
    template <typename... Ts>
    struct Archetype {
        using canonical_types = typename meta::sort_types<meta::type_list<Ts...>>::type;
        
        static constexpr size_t size = (sizeof(Ts) + ...);
        static constexpr size_t length = sizeof...(Ts);
    };

    struct RTArchetype {
        const std::size_t m_index;
        std::span<const meta::type_id_t> m_types; // For matching against query.
    };

    class ArchetypeRegistry {
    public:
        // Retrieve index from global registry.
        template <typename Arch> static const size_t get() {
            std::lock_guard lock(m_mutex);

            static const size_t index = [&] {
                const size_t i = m_archetypes.size();

                m_archetypes.push_back({.m_index = i, .m_types = std::span(Arch::canonical_types::ids)});
                return i;
            }();

            return index;
        };

        static std::span<const RTArchetype> all() {
            return m_archetypes;
        }

    private:
        static inline std::mutex m_mutex;
        static inline std::vector<RTArchetype> m_archetypes;
        // std::vector<Archetype> m_archetypes;
    };
}
