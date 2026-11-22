#pragma once 

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace platform { 
  struct alignas(64) PageBlock {
    size_t m_waterline = 0; // Word index till where the bitset is completely saturated.  
    size_t m_claim = 0; // What thread claimed this block for structural access. 

    uint8_t* data() {
      return reinterpret_cast<uint8_t*>(this) + sizeof(PageBlock);
    }
  };
}
