#pragma once 

#include "meta/type_id.hpp"
#include "world/allocator.hpp"
#include "world/archetype.hpp"
#include <atomic>
#include <meta/filter.hpp>
#include <iostream>

namespace world {
  template <size_t World, typename... Ts> struct Query {
    using parsed = filter::Parse<meta::type_list<>, meta::type_list<>, meta::type_list<>, Ts...>::type;
    using queries = parsed::queries;
    using withs = parsed::withs;
    using withouts = parsed::withouts;

    // O_O 
    inline static const Signature querySig = []<typename... Cs>(meta::type_list<Cs...>) {
          Signature sig;
          (sig.set(meta::TypeID<World, Cs>::index), ...);
          return std::move(sig);
        }(typename Query<World, Ts...>::queries{});
    // queries is a meta::type_ist hence why it needs the extra lambda to expand it into the pack.
    inline static const Signature includeSig = []<typename... Cs>(meta::type_list<Cs...>) {
          Signature sig;
          (sig.set(meta::TypeID<World, Cs>::index), ...);
          return std::move(sig);
        }(typename Query<World, Ts...>::withs{});

    inline static const Signature excludeSig = []<typename... Cs>(meta::type_list<Cs...>) {
          Signature sig;
          (sig.set(meta::TypeID<World, Cs>::index), ...);
          return std::move(sig);
        }(typename Query<World, Ts...>::withouts{});

    // Combined include signature
    inline static const Signature CIncludeSig = querySig | includeSig;
  };
  
  // Runtime instance
  struct QueryInstance {
    std::vector<ArchetypePool*> m_pools;
  };
}
