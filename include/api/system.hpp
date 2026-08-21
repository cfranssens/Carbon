#include "api/core.hpp"
#include <array>
#include <cstddef>
#include <utility>

namespace api {
  template <size_t World> class Core;

  // Inherit methods from the correct Core based on runtime m_worldIndex. 
  // This allows dyanmically passing the system to different Core registrars. 
  #define ECS_MAKE_SYSTEM_DISPATCH(ReturnType, MethodName) \
    template <typename... Ts> ReturnType MethodName(Ts&&... args) { \
      static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) { \
        using Func = ReturnType(*)(Ts&&...); \
        return std::array<Func, sizeof...(Is)>{{ \
        &api::Core<Is>::template MethodName<Ts...>... \
      }}; \
    }(std::make_index_sequence<ECS_MAX_WORLDS>{}); \
    return dispatch[m_worldIndex](std::forward<Ts>(args)...); \
  }

  // Function that takes thread argument. 
  #define ECS_MAKE_THREADED_SYSTEM_DISPATCH(ReturnType, MethodName) \
    template <size_t Thread, typename... Ts> ReturnType MethodName(Ts&&... args) { \
      static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) { \
        using Func = ReturnType(*)(Ts&&...); \
        return std::array<Func, sizeof...(Is)>{{ \
        &api::Core<Is>::template MethodName<Thread, Ts...>... \
      }}; \
    }(std::make_index_sequence<ECS_MAX_WORLDS>{}); \
    return dispatch[m_worldIndex](std::forward<Ts>(args)...); \
  }

  // Base System class to derive from.
  class System {
    public: 
      virtual void initialise() {}
      virtual void update(float dt) {}

    protected: 
      ECS_MAKE_THREADED_SYSTEM_DISPATCH(world::Entity, createEntity);

      template <size_t Thread, typename... Ts> void addComponents(world::Entity e, Ts&&... components) {
        static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) {
          using Func = void(*)(world::Entity&, Ts&&...);
          return std::array<Func, sizeof...(Is)>{{
            &api::Core<Is>::template addComponents<Thread, Ts...>...
          }};
        }(std::make_index_sequence<ECS_MAX_WORLDS>{});
        return dispatch[m_worldIndex](std::forward<world::Entity&>(e), std::forward<Ts>(components)...);
      } 

      template <size_t Thread, typename... Ts> void removeComponents(world::Entity e) {
        static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) {
          using Func = void(*)(world::Entity&);
          return std::array<Func, sizeof...(Is)>{{
            &api::Core<Is>::template removeComponents<Thread, Ts...>...
          }};
        }(std::make_index_sequence<ECS_MAX_WORLDS>{});
        return dispatch[m_worldIndex](std::forward<world::Entity&>(e));
      } 

      template <typename = void>
      void deleteEntity(world::Entity e) {
          static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) {
            using Func = void(*)(world::Entity);
            return std::array<Func, sizeof...(Is)>{{
              &api::Core<Is>::deleteEntity...
            }};
          }(std::make_index_sequence<ECS_MAX_WORLDS>{});
          dispatch[m_worldIndex](std::forward<world::Entity>(e));
      }
    
      template <size_t Thread, typename... Ts, typename Fn> void query(Fn&& fn) {
        static constexpr auto dispatch = []<size_t... Is>(std::index_sequence<Is...>) {
          using Func = void(*)(Fn&&);
          return std::array<Func, sizeof...(Is)>{{
            &api::Core<Is>::template query<Thread, Ts...>...
          }};
        }(std::make_index_sequence<ECS_MAX_WORLDS>{});
        dispatch[m_worldIndex](std::forward<Fn>(fn));
      } 

    private: 
      size_t m_worldIndex = 0; // Default, gets overriden on instantiation. 
      template <size_t W> friend class Core;
  };
}
