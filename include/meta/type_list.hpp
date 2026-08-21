#pragma once 

#include <cstddef>
#include <cstdint>
#include <tuple>

namespace meta {
  // Hash over __PRETTY_FUNCTION__ to define a canonical order. 
  constexpr uint64_t ct_hash(const char* s) {
    uint64_t h = 1469598103834665603ull;
    while (*s) { h = (h ^ uint64_t(*s++)) * 1099511628211ull; }
    return h;
  }

  // Why MSVC gotta use a different ABI standard :(
  template <typename T> constexpr uint64_t type_id() {
    #if defined(_MSC_VER) 
      return ct_hash(__FUNCSIG__);
    #else 
      return ct_hash(__PRETTY_FUNCTION__); 
    #endif
  }

  // Literal magic
  template <typename... Ts> struct type_list {
    template <typename T> using prepend = type_list<T, Ts...>;
    static constexpr size_t length = sizeof...(Ts);
    using tuple = std::tuple<Ts...>;
  };

  template <typename T, typename List> struct insert_sorted;
  template <typename T> struct insert_sorted<T, type_list<>> { using type = type_list<T>; };
  template <typename T, typename Head, typename... Tail>
  struct insert_sorted<T, type_list<Head, Tail...>> {
    using type = std::conditional_t<(type_id<T>() < type_id<Head>()), 
      type_list<T, Head, Tail...>, 
      typename insert_sorted<T, type_list<Tail...>>::type::template prepend<Head>>;
  };

  template <typename List> struct sort_types;
  template <> struct sort_types<type_list<>> { using type = type_list<>; };
  template <typename Head, typename... Tail>
  struct sort_types<type_list<Head, Tail...>> {
    using type = typename insert_sorted<Head, typename sort_types<type_list<Tail...>>::type>::type;
  };

  template <typename T, typename List> struct index_of;
  template <typename T, typename... Ts> 
  struct index_of<T, type_list<T, Ts...>> : std::integral_constant<size_t, 0> {};
  template <typename T, typename Head, typename... Tail>
  struct index_of<T, type_list<Head, Tail...>> : std::integral_constant<size_t, 1 + index_of<T, type_list<Tail...>>::value> {};

  template <typename List1, typename List2> struct concat;

  template <typename... T1s, typename... T2s> struct concat<type_list<T1s...>, type_list<T2s...>> {
    using type = type_list<T1s..., T2s...>;
  };

  template <typename List1, typename List2>
  using concat_t = typename concat<List1, List2>::type;

  template <typename List> struct for_each_type;
  template <typename... Ts> struct for_each_type<type_list<Ts...>> {
    template <typename F> static constexpr void apply(F &&f) {
        [&]<size_t... Is>(std::index_sequence<Is...>) {
            (f.template operator()<Ts, Is>(), ...);
        }(std::make_index_sequence<sizeof...(Ts)>{});
      }
  };

  template <typename Decl, typename Canon> struct permutation;
  template <typename... Ds, typename... Cs>
  struct permutation<type_list<Ds...>, type_list<Cs...>> {
    static constexpr std::array<size_t, sizeof...(Cs)> value = { index_of<Cs, type_list<Ds...>>::value... };
  };
} // namespace world::meta}
