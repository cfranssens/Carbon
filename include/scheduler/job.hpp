#pragma once
#include <type_traits>

namespace scheduler {

  // Untyped function pointer + payload contianer.
  struct Task {
    using Fn = void(*)(void*);
    
    Fn m_fn;
    void *m_payload;

    // Execute 
    void run() {
      m_fn(m_payload);  
    }

    template <typename F>
    Task(F& f) {
      using FnT = std::decay_t<F>;
    
      m_fn = [] (void* p) {
        *static_cast<FnT*>(p)();
      };

      m_payload = &f;
    }
  };
}
