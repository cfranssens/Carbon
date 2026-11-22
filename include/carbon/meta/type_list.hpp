#pragma once

#include <array>
#include <stddef.h>
#include <stdint.h>
#include <tuple>
#include <type_traits>
#include <utility>

namespace meta {
  constexpr uint64_t ct_hash(const char* s, uint64_t h = 1469598103834665603ull) {
    return *s ? ct_hash(s + 1, (h ^ uint64_t(*s)) * 1099511628211ull) : h;
  }

  template <typename T> constexpr uint64_t type_id() {
  #if defined(_MSC_VER)
    return ct_hash(__FUNCSIG__);
  #else
    return ct_hash(__PRETTY_FUNCTION__);
  #endif
  }

  template <typename... Ts> struct type_list {
    template <typename T> using prepend = type_list<T, Ts...>;
    static constexpr size_t length = sizeof...(Ts);
    using tuple = std::tuple<Ts...>;
  };
  
  template <typename... Ts> constexpr size_t type_list<Ts...>::length;

  template <size_t... Is> struct index_list {};

  template <typename T, typename List> struct insert_sorted;
  template <typename T> struct insert_sorted<T, type_list<>> { using type = type_list<T>; };
  template <typename T, typename Head, typename... Tail>
  
  struct insert_sorted<T, type_list<Head, Tail...>> {
    using type = typename std::conditional<(type_id<T>() < type_id<Head>()),
      type_list<T, Head, Tail...>,
      typename insert_sorted<T, type_list<Tail...>>::type::template prepend<Head>>::type;
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
  struct index_of<T, type_list<Head, Tail...>>
    : std::integral_constant<size_t, 1 + index_of<T, type_list<Tail...>>::value> {};

  template <typename List1, typename List2> struct concat;
  template <typename... T1s, typename... T2s> struct concat<type_list<T1s...>, type_list<T2s...>> {
    using type = type_list<T1s..., T2s...>;
  };
  template <typename List1, typename List2>
  using concat_t = typename concat<List1, List2>::type;

  template <typename List, size_t I = 0> struct for_each_type;
  template <size_t I> struct for_each_type<type_list<>, I> {
    template <typename F> static void apply(F&&) {}
  };
  template <typename T, typename... Ts, size_t I>
  struct for_each_type<type_list<T, Ts...>, I> {
    template <typename F> static void apply(F&& f) {
      f.template operator()<T, I>();
      for_each_type<type_list<Ts...>, I + 1>::apply(std::forward<F>(f));
    }
  };

  template <typename Decl, typename Canon> struct permutation;
  template <typename... Ds, typename... Cs>
  struct permutation<type_list<Ds...>, type_list<Cs...>> {
    static constexpr std::array<size_t, sizeof...(Cs)> value = {{ index_of<Cs, type_list<Ds...>>::value... }};
  };
  template <typename... Ds, typename... Cs>
  constexpr std::array<size_t, sizeof...(Cs)> permutation<type_list<Ds...>, type_list<Cs...>>::value;

  template <typename... Ts> struct size_sum;
  template <> struct size_sum<> { static constexpr size_t value = 0; };
  template <typename T, typename... Ts> struct size_sum<T, Ts...> {
    static constexpr size_t value = sizeof(T) + size_sum<Ts...>::value;
  };

  constexpr size_t align_up(size_t n, size_t a) { return (n + a - 1) / a * a; }

  namespace detail {
    template <size_t Cap, size_t Align, size_t Cum, typename Acc, typename Types> struct offsets_impl;

    template <size_t Cap, size_t Align, size_t Cum, size_t... Offs>
    struct offsets_impl<Cap, Align, Cum, index_list<Offs...>, type_list<>> {
      static constexpr size_t value[sizeof...(Offs)] = { Offs... };
    };
    template <size_t Cap, size_t Align, size_t Cum, size_t... Offs>
    constexpr size_t offsets_impl<Cap, Align, Cum, index_list<Offs...>, type_list<>>::value[sizeof...(Offs)];

    template <size_t Cap, size_t Align, size_t Cum, size_t... Offs, typename T, typename... Ts>
    struct offsets_impl<Cap, Align, Cum, index_list<Offs...>, type_list<T, Ts...>>
      : offsets_impl<Cap, Align, Cum + align_up(Cap * sizeof(T), Align),
                     index_list<Offs..., Cum>, type_list<Ts...>> {};
  }

  template <size_t Cap, size_t Align, typename List>
  struct canonical_offsets
    : detail::offsets_impl<Cap, Align, align_up(Cap >> 2, Align), index_list<>, List> {};
}
