#pragma once 

#include "meta/type_id.hpp"
#include "meta/type_list.hpp"
#include "platform/pageblock.hpp"
#include "scheduler/job.hpp"
#include "world/allocator.hpp"
#include "world/archetype.hpp"
#include "world/entity.hpp"
#include <atomic>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <world/query.hpp>
#include <api/system.hpp>



namespace api {
  // Apparently static inline in the header itself doesn't gueard for MT as it would in the function boddy. 
  // CreateEntity already guards MT making the extra guard redundant.
   template <size_t World, size_t Thread, typename Arch> struct ArchetypeStorage {
    inline static world::ArchetypePool* m_pool = nullptr;

    inline static size_t m_poolIndex;
    inline static size_t m_blockIndex;

    static inline platform::PageBlock* get(world::ArenaAllocator<world::ArchetypePool>& pools) {

    [[unlikely]] if (!m_pool) {
      world::Signature sig; 
      meta::for_each_type<typename Arch::canonical_types>::apply([&]<typename T, size_t I>() {
        sig.set(meta::TypeID<World, T>::index);
      });

      m_pool = [&] {
        for (size_t i = 0; i < pools.template count<Thread>(); ++i) {
          if (pools.template at<Thread>(i)->m_signature == sig) {
            m_poolIndex = i;
            return pools.template at<Thread>(i);
          }
        }
        m_poolIndex = pools.count<Thread>();
        return pools.create<Thread>();
      }();

      m_pool->m_layout = Arch::template layoutInstance<World>();

      // PAGEBLOCK_SIZE / 2 is the maximum needed clear region. 
      m_blockIndex = m_pool->m_blocks.count<Thread>(); 
      m_pool->m_blocks.template create<Thread>(PAGEBLOCK_SIZE / 2);
      m_pool->m_waterlines[Thread] = 0; // Reset on init


      m_pool->m_signature = std::move(sig);
    }
    
    return m_pool->m_blocks.at<Thread>(m_pool->m_waterlines[Thread]);
  }
};
  
  // Core class per World, the template is used to differentiate static initialisation across instances.
  template <size_t World> class Core {
    public:
      template <typename T> static void registerSystem() {
        static_assert(std::is_base_of<System, T>(), "T must be derived from System!");
        std::unique_ptr<System> sys = std::make_unique<T>();
        sys->m_worldIndex = World;
        sys->initialise();
        m_systems.emplace_back(std::move(sys));
      }
      static void registerSystem(std::unique_ptr<api::System> sys) {
        sys->m_worldIndex = World;
        sys->initialise();
        m_systems.emplace_back(std::move(sys));
      }

      // Run all update() calls once
      static void tick(float dt) {
        for (auto& sys: m_systems) {
          sys->update(dt);
        }
      }

      // TODO! reset static vars
      static void reset() {
    
      }


    private:
      template <size_t Thread, typename... Ts> static world::Entity createEntity(Ts&&... components) {
        using Arch = world::Archetype<std::remove_cvref_t<Ts>...>;
        using CanonList = typename Arch::canonical_types;
    
        // Create pool for unique Ts... 
        static platform::PageBlock* block = ArchetypeStorage<World, Thread, Arch>::get(m_pools);
        // Scan for free space from the waterline (aggregate O(1))
        [[unlikely]] if (block->m_count >= Arch::capacity) {
          block = ArchetypeStorage<World, Thread, Arch>::m_pool->m_blocks.template create<Thread>(Arch::bitsetRegion);
          ArchetypeStorage<World, Thread, Arch>::m_pool->m_waterlines[Thread]++;
        } 
    
        uint64_t* scanWord = reinterpret_cast<uint64_t*>(block->data()) + block->m_waterline; // First bitset, live
        while (*scanWord == UINT64_MAX) {block->m_waterline++; scanWord++;} // Guaranteed to exit because of earlier check. 
        size_t shift = __builtin_ctzll(~*scanWord);
        *scanWord |= 1ULL << shift; // Set bit
        block->m_count++;
      
        // Index within the block. With an assumed PAGEBLOCK_SIZE of 2MiB this is 21 bits. 24 bits are used to allow PAGEBLOCKs a size up to 16MiB.
        uint64_t blockIndex = block->m_waterline << 6 | shift;

        // World is ommited in the ID because an ID from a different World cannot be safely obtained within a System. 
        // [Thread(8)][Arch(16)][BlockIndex(16)][index(24)]
        uint64_t index = (Thread << 56ULL) | (ArchetypeStorage<World, Thread, Arch>::m_poolIndex << 40ULL) | (ArchetypeStorage<World, Thread, Arch>::m_pool->m_waterlines[Thread] << 24ULL) | blockIndex;
        std::byte* const basePtr = block->data();
        
        // Fold placement write
        [&]<size_t... Is>(std::index_sequence<Is...>) {
        (..., new (reinterpret_cast<std::remove_cvref_t<Ts>*>(
            basePtr + Arch::template componentRegions<World>[
                meta::typeIndexPermutation<World, meta::type_list<std::remove_cvref_t<Ts>...>>[Is]
            ]) + blockIndex)
            std::remove_cvref_t<Ts>(std::forward<Ts>(components)));
          }(std::index_sequence_for<Ts...>{});

        return index;
      }

      // Invalidates old entity ID
      template<size_t Thread, typename... Ts> static void addComponents(world::Entity& e, Ts&&... components) {
        uint64_t thread = e >> 56ULL;
        uint64_t arch = (e >> 40ULL) & 0xFFFFULL;
        uint64_t blockIndex = (e >> 24ULL) & 0xFFFFULL;
        uint64_t index = e & 0xFFFFFFULL;

        uint64_t wordIndex = index >> 6;
        uint64_t shift = index & 63; 

        world::ArchetypePool* pool = m_pools.at(thread, arch);
        platform::PageBlock* block = pool->m_blocks.at(thread, blockIndex);
          
        static std::vector<meta::TypeInfo> typeInfos = [] {
          std::vector<meta::TypeInfo> typeInfos;
          (typeInfos.emplace_back(meta::TypeID<World, std::remove_cvref_t<Ts>>::instance()), ...);
          return typeInfos;
        }();

        static const world::Signature sig = [] {
          world::Signature sig;
          (sig.set(meta::TypeID<World, std::remove_cvref_t<Ts>>::index), ...);
          return std::move(sig);
        }();
    

        size_t dstPoolIndex = 0;
        world::ArchetypePool* dstPool = [&] {
          const world::Signature targetSig = sig | pool->m_signature;
          // Search locally 
          for (dstPoolIndex = 0; dstPoolIndex < pool->m_edgePools.template count<Thread>(); ++ dstPoolIndex) {
            world::ArchetypePool* dstPool = *pool->m_edgePools.at<Thread>(dstPoolIndex);
            if (dstPool->m_signature == targetSig) {
              return dstPool;
            } 
          }

          // Search globally 
          world::ArchetypePool* target = nullptr;
          for (dstPoolIndex = 0; dstPoolIndex < m_pools.template count<Thread>(); ++dstPoolIndex) {
            // std::cout << std::bitset<32>(m_pools.template at<Thread>(i)->m_signature.m_words[0]) << " u " << std::bitset<32>(targetSig.m_words[0]) << std::endl; 
            if (m_pools.template at<Thread>(dstPoolIndex)->m_signature == targetSig) {
              // REgister locally 
              return *pool->m_edgePools.create<Thread>(m_pools.template at<Thread>(dstPoolIndex));
            }
          }

          // Create 
          std::vector<meta::TypeInfo> mergedInfos = typeInfos;
          mergedInfos.insert( mergedInfos.end(), pool->m_layout.typeInfos.begin(), pool->m_layout.typeInfos.end() );

          world::PageblockLayoutInstance layout(mergedInfos);
          world::ArchetypePool* dstPool = m_pools.create<Thread>();
          // DstPoolIndex Should be at the end of m_pools atp
          dstPool->m_layout = layout;
          dstPool->m_waterlines[Thread] = 0;
      //std::cout << dstPool->m_waterlines[Thread] << std::endl;
          
          dstPool->m_signature = sig | pool->m_signature;
          dstPool->m_blocks.create<Thread>(layout.bitsetRegion);
          return *pool->m_edgePools.create<Thread>(dstPool);
        }();

        platform::PageBlock* dstBlock = dstPool->m_blocks.at<Thread>(dstPool->m_waterlines[Thread]);
        [[unlikely]] if (dstBlock->m_count >= dstPool->m_layout.capacity) {
          dstBlock = dstPool->m_blocks.create<Thread>(dstPool->m_layout.bitsetRegion);
          dstPool->m_waterlines[Thread]++;
        }
    
        uint64_t* scanWord = reinterpret_cast<uint64_t*>(dstBlock->data()) + dstBlock->m_waterline; // First bitset, live
        while (*scanWord == UINT64_MAX) {dstBlock->m_waterline++; scanWord++;} // Guaranteed to exit because of earlier check. 

        size_t dstShift = __builtin_ctzll(~*scanWord);
        *scanWord |= 1ULL << dstShift; // Set bit
        dstBlock->m_count++;
        
        // Index within the block. With an assumed PAGEBLOCK_SIZE of 2MiB this is 21 bits. 24 bits are used to allow PAGEBLOCKs a size up to 16MiB.
        uint64_t dstBlockIndex = dstBlock->m_waterline << 6 | dstShift;
        // Copy old componetns
        std::byte* const srcBasePtr = block->data();
        std::byte* const dstBasePtr = dstBlock->data();
        
        for (size_t i = 0; i < pool->m_layout.length; ++i) {
          auto& type = pool->m_layout.typeInfos[i];
    
          size_t dstIndex = 0;
          for (; dstIndex < dstPool->m_layout.length; ++dstIndex) {
            if (dstPool->m_layout.typeInfos[dstIndex].index == type.index) break;
          }
    
          uint64_t srcOffset = pool->m_layout.componentRegions[i] + (index * type.size);
          uint64_t dstOffset = dstPool->m_layout.componentRegions[dstIndex] + (dstBlockIndex * type.size);

          copyType(type, srcBasePtr + srcOffset, dstBasePtr + dstOffset);
        }

        [&]<size_t... Is>(std::index_sequence<Is...>) {(..., [&]() {
          size_t dstIndex = 0;
          for (; dstIndex < dstPool->m_layout.length; ++dstIndex) {
            if (dstPool->m_layout.typeInfos[dstIndex].index == meta::TypeID<World, Ts>::index) break;
          }
        
          uint64_t dstOffset = dstPool->m_layout.componentRegions[dstIndex] + (dstBlockIndex * sizeof(Ts));
          new (reinterpret_cast<std::remove_cvref_t<Ts>*>(dstBasePtr + dstOffset)) std::remove_cvref_t<Ts>(std::forward<Ts>(components));
        }());
        }(std::index_sequence_for<Ts...>{});

        // Remove old entity
        uint64_t* word = reinterpret_cast<uint64_t*>(block->data()) + wordIndex;
        *word &= ~(1ULL << shift);
          
        block->m_waterline = wordIndex; // Reset within block
        pool->m_waterlines[thread] = blockIndex;

        e =  (Thread << 56ULL) | (dstPoolIndex << 40ULL) | (dstPool->m_waterlines[Thread] << 24ULL) | dstBlockIndex;
      }

      template<size_t Thread, typename... Ts> static void removeComponents(world::Entity& e) {
        uint64_t thread = e >> 56ULL;
        uint64_t arch = (e >> 40ULL) & 0xFFFFULL;
        uint64_t blockIndex = (e >> 24ULL) & 0xFFFFULL;
        uint64_t index = e & 0xFFFFFFULL;

        uint64_t wordIndex = index >> 6;
        uint64_t shift = index & 63;

        world::ArchetypePool* pool = m_pools.at<Thread>(arch);
        platform::PageBlock* block = pool->m_blocks.at<Thread>(blockIndex);

        static const world::Signature sig = [] {
          world::Signature sig;
          (sig.set(meta::TypeID<World, std::remove_cvref_t<Ts>>::index), ...);
          return sig;
        }();

        // Target signature depends on the runtime pool the entity currently lives in, so unlike `sig`
        // this can't be cached statically.
        world::Signature targetSig = pool->m_signature;
        (targetSig.clear(meta::TypeID<World, std::remove_cvref_t<Ts>>::index), ...);

        size_t dstPoolIndex = 0;
        world::ArchetypePool* dstPool = [&] {
          // Search locally
          for (dstPoolIndex = 0; dstPoolIndex < pool->m_edgePools.template count<Thread>(); ++dstPoolIndex) {
            world::ArchetypePool* dstPool = *pool->m_edgePools.at<Thread>(dstPoolIndex);
            if (dstPool->m_signature == targetSig) {
              return dstPool;
            }
          }

          // Search globally
          for (dstPoolIndex = 0; dstPoolIndex < m_pools.template count<Thread>(); ++dstPoolIndex) {
            if (m_pools.template at<Thread>(dstPoolIndex)->m_signature == targetSig) {
              // Register locally
              return *pool->m_edgePools.create<Thread>(m_pools.template at<Thread>(dstPoolIndex));
            }
          }

          // Create - keep every existing component whose type index isn't in `sig`.
          std::vector<meta::TypeInfo> remainingInfos;
          remainingInfos.reserve(pool->m_layout.length);
          for (auto& type : pool->m_layout.typeInfos) {
            if (!sig.test(type.index)) {
              remainingInfos.push_back(type);
            }
          }

          world::PageblockLayoutInstance layout(remainingInfos);
          world::ArchetypePool* dstPool = m_pools.create<Thread>();
          dstPoolIndex++; // Should be at the end of m_pools atp
          dstPool->m_layout = layout;
          dstPool->m_waterlines[Thread] = 0;

          dstPool->m_signature = targetSig;
          dstPool->m_blocks.create<Thread>(layout.bitsetRegion);
          return *pool->m_edgePools.create<Thread>(dstPool);
        }();

        platform::PageBlock* dstBlock = dstPool->m_blocks.at<Thread>(dstPool->m_waterlines[Thread]);
        [[unlikely]] if (dstBlock->m_count >= dstPool->m_layout.capacity) {
          dstBlock = dstPool->m_blocks.create<Thread>(dstPool->m_layout.bitsetRegion);
          dstPool->m_waterlines[Thread]++;
        }

        uint64_t* scanWord = reinterpret_cast<uint64_t*>(dstBlock->data()) + dstBlock->m_waterline;
        while (*scanWord == UINT64_MAX) {dstBlock->m_waterline++; scanWord++;}

        size_t dstShift = __builtin_ctzll(~*scanWord);
        *scanWord |= 1ULL << dstShift;
        dstBlock->m_count++;

        uint64_t dstBlockIndex = dstBlock->m_waterline << 6 | dstShift;

        std::byte* const srcBasePtr = block->data();
        std::byte* const dstBasePtr = dstBlock->data();

        // Copy only the surviving components. Components being removed are simply skipped here -
        // they are neither copied nor destroyed, consistent with the note above.
        for (size_t i = 0; i < pool->m_layout.length; ++i) {
          auto& type = pool->m_layout.typeInfos[i];
          if (sig.test(type.index)) continue; // being removed

          size_t dstIndex = 0;
          for (; dstIndex < dstPool->m_layout.length; ++dstIndex) {
            if (dstPool->m_layout.typeInfos[dstIndex].index == type.index) break;
          }

          uint64_t srcOffset = pool->m_layout.componentRegions[i] + (index * type.size);
          uint64_t dstOffset = dstPool->m_layout.componentRegions[dstIndex] + (dstBlockIndex * type.size);

          copyType(type, srcBasePtr + srcOffset, dstBasePtr + dstOffset);
        }

        // Remove old entity - just clear the bit, leave the old block's memory (including the removed
        // components' storage) untouched.
        uint64_t* word = reinterpret_cast<uint64_t*>(block->data()) + wordIndex;
        *word &= ~(1ULL << shift);

        block->m_waterline = wordIndex; // Reset within block
        pool->m_waterlines[thread] = blockIndex;

        e = (Thread << 56ULL) | (dstPoolIndex << 40ULL) | (dstPool->m_waterlines[Thread] << 24ULL) | dstBlockIndex;
      }
   
      static void deleteEntity(world::Entity e) {
          uint64_t thread = e >> 56ULL;
          uint64_t arch = (e >> 40ULL) & 0xFFFFULL;
          uint64_t blockIndex = (e >> 24ULL) & 0xFFFFULL;
          uint64_t index = e & 0xFFFFFFULL;

          uint64_t wordIndex = index >> 6;
          uint64_t shift = index & 63; 

          world::ArchetypePool* pool = m_pools.at(thread, arch);
          platform::PageBlock* block = pool->m_blocks.at(thread, blockIndex);
          
          uint64_t* word = reinterpret_cast<uint64_t*>(block->data()) + wordIndex;
          *word &= ~(1ULL << shift);
          
          block->m_waterline = wordIndex; // Reset within block
          pool->m_waterlines[thread] = blockIndex;
      }
       
      // The thread here only determines on what thread the query will be evaluated to avoid a loop incrementing an atomic. 
      template <size_t Thread, typename... Ts, typename Fn> static void query(Fn&& fn) {
        using Query = world::Query<World, Ts...>;
        using QueryComponents = typename Query::queries;
        
        static world::QueryInstance* instance = m_queries.create<Thread>(); // Arena allocator for thread safe stable addresses.  
        static std::vector<scheduler::QueryJobPayload<Fn, Ts...>> payloads;
        // querySignature + includeSig - excludeSig
        [&]<typename... Qs>(meta::type_list<Qs...>) {
          static std::vector<scheduler::QueryJobPayload<Fn, Qs...>> payloads;
        
          static size_t waterline = 0; 
          for (; waterline < m_pools.count<Thread>(); waterline++) {
            if (world::Signature::match(m_pools.at<Thread>(waterline)->m_signature, Query::CIncludeSig, Query::excludeSig)) {      
              static const auto perm = meta::typeIndexPermutation<World, typename Query::queries>;
                
              // Note: The offsets array must now be sized to sizeof...(Qs) instead of sizeof...(Ts)
              std::array<size_t, sizeof...(Qs)> offsets;
              world::ArchetypePool*& pool = instance->m_pools.emplace_back(m_pools.at<Thread>(waterline));
              world::Signature sig = Query::querySig;
                
              for (int i = 0; i < perm.size(); i++) {
                size_t index = sig.ctz();
                size_t relativeIndex = m_pools.at<Thread>(waterline)->m_signature.popcount(index);
                offsets[perm[i]] = pool->m_layout.componentRegions[relativeIndex];
                sig.clear(index);
              } 
                
              size_t blockIndex = 0; 
              for (auto it = pool->m_blocks.template begin<Thread>(); it != pool->m_blocks.template end<Thread>(); ++it) {
                platform::PageBlock* block = &(*it); // Trust me bro
                uint64_t baseId = (static_cast<uint64_t>(Thread) << 56ULL) | ((waterline & 0xFFFFULL) << 40ULL) | ((blockIndex++ & 0xFFFFULL) << 24ULL);
                payloads.push_back({ block, offsets, nullptr, baseId});
              }
            }
          }

          auto jobWrapper = [](void* userData) {
            auto* payload = static_cast<scheduler::QueryJobPayload<Fn, Qs...>*>(userData);  
            executeBlock<std::remove_pointer_t<decltype(payload)>, Fn, Qs...>(payload, std::index_sequence_for<Qs...>{});
          };

          for (auto& payload : payloads) {
            payload.m_userFn = &fn; 
            jobWrapper(&payload); // Execute immediately for now. 
          }
        } (typename Query::queries{});      
      }

      static inline void copyType(meta::TypeInfo& info, void* src, void* dst) {
        if (info.triviallyCopyable) {
          std::memcpy(dst, src, info.size);
        } else {
          info.copyConstruct(dst, src);
          info.destroy(src);
        }
      }

      // Outer level is monotonic
      static inline world::ArenaAllocator<world::ArchetypePool> m_pools;

      // All underlying system instances. 
      static inline world::ArenaAllocator<world::QueryInstance> m_queries;
      static inline std::vector<std::unique_ptr<System>> m_systems;

      friend class System; // EXpose macro to private methods. 
  }; 

}
