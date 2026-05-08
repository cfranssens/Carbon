#pragma once

#include "archetype.hpp"
#include "scheduler/job.hpp"
#include <vector>

namespace world {

template <typename... Ts> class Query {
  public:
    Query() {
      m_inclusionFilter.m_bitset |= Archetype<Ts...>::signature.m_bitset;
    };

    ~Query() {};

    // Modify existing query object in-place 
    template <typename... Is> Query<Ts...>& with() {
      m_inclusionFilter.m_bitset |= Archetype<Is...>::signature.m_bitset;
      return *this;
    }
    template <typename ...Es> Query<Ts...>& without() {
      m_exclusionFilter.m_bitset |= Archetype<Es...>::signature.m_bitset;      
      return *this;
    }

    template <typename F> void run(F& f) {
      scheduler::Task task(f);
    }

  private: 
    // Inefficent but Queries are cached. 
    Signature m_inclusionFilter;
    Signature m_exclusionFilter;
    // To be explanded by scheduler 
    std::vector<scheduler::Task> m_tasks; 
};
} // namespace world``
