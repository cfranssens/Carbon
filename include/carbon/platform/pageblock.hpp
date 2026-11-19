#pragma once 

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace platform { 
  struct alignas(64) PageBlock {
    size_t m_waterline = 0; // Word index till where the bitset is completely saturated.  
   
    uint8_t* data() {
      return reinterpret_cast<uint8_t*>(this) + sizeof(PageBlock);
    }
  };
}
