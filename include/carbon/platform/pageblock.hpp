#pragma once 

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace platform { 
  struct alignas(64) PageBlock {
    size_t m_waterline = 0; // Word index till where the bitset is completely saturated. 

    PageBlock(size_t clearRegion = 0) {
      if (clearRegion > 0) {
        memset(data(), 0, clearRegion);
      }
    }; 
   
    uint8_t* data() {
      return reinterpret_cast<uint8_t*>(this) + sizeof(PageBlock);
    }

    void clear(size_t bitsetRegion) {
        m_waterline = 0;
        
        memset(data(), 0, bitsetRegion);
    }
  };
}
