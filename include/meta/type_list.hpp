#pragma once


#include "meta/type_id.hpp"
#include <array>
#include <cstddef>

#include <ostream>
#include <tuple>

namespace world::meta {
    // very meta
    template <typename... Ts> struct type_list {
        template <typename T> using prepend = type_list<T, Ts...>;

        static constexpr size_t length = sizeof...(Ts);
        static constexpr std::array<type_id_t, length> ids {type_id<Ts>()...}; 
        using tuple = std::tuple<Ts...>;
    };

    template <typename T, typename List> struct insert_sorted;
    template <typename T> struct insert_sorted<T, type_list<>> {using type = type_list<T>;};

    // Umm, what the fuck
    template <typename T, typename Head, typename... Tail>
    struct insert_sorted<T, type_list<Head, Tail...>> {using type = std::conditional_t<(type_id<T>() < type_id<Head>()), type_list<T, Head, Tail...>, typename insert_sorted<T, type_list<Tail...>>::type::template prepend<Head>>;};

    template <typename List> struct sort_types;
    template <> struct sort_types<type_list<>> {using type = type_list<>;};
    template <typename Head, typename... Tail>struct sort_types<type_list<Head, Tail...>> {using type = typename insert_sorted<Head, typename sort_types<type_list<Tail...>>::type>::type;};

    template <typename T, typename List>
    struct index_of;

    template <typename T, typename... Ts>
    struct index_of<T, type_list<T, Ts...>>
    : std::integral_constant<std::size_t, 0> {};

    template <typename T, typename Head, typename... Tail>
    struct index_of<T, type_list<Head, Tail...>>
    : std::integral_constant<std::size_t, 1 + index_of<T, type_list<Tail...>>::value> {};

    template <typename List> struct for_each_type_indexed;

    template <typename... Ts> struct for_each_type_indexed<type_list<Ts...>> {
        template <typename F> static constexpr void apply(F &&f) {
            apply_impl(std::forward<F>(f), std::index_sequence_for<Ts...>{});
        }

    private:
        template <typename F, size_t... Is>
        static constexpr void apply_impl(F &&f, std::index_sequence<Is...>) {
            (f.template operator()<Ts, Is>(), ...);
        }
    };

    template <typename Decl, typename Canon>
    struct permutation;

    template <typename... Ds, typename... Cs>
    struct permutation<type_list<Ds...>, type_list<Cs...>> {
        static constexpr std::array<std::size_t, sizeof...(Cs)> value = {
            index_of<Cs, type_list<Ds...>>::value...
        };
    };

    template <typename DeclList, typename CanonTuple>
    constexpr auto permute_from_canonical(CanonTuple&& canon) {
        using CanonList = typename sort_types<DeclList>::type;
        constexpr auto& perm = permutation<DeclList, CanonList>::value;

        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return typename DeclList::tuple{
                std::get<perm[Is]>(std::forward<CanonTuple>(canon))...
            };
        }(std::make_index_sequence<DeclList::length>{});
    }

    template <typename Decl, typename Canon>
    struct decl_to_canon;

    template <typename... Ds, typename Cs>
    struct decl_to_canon<type_list<Ds...>, Cs> {
        static constexpr std::array<std::size_t, sizeof...(Ds)> value = {
            index_of<Ds, Cs>::value...
        };
    };

}

// Formatter overrides

#ifndef NDEBUG
template <std::size_t N>
struct std::formatter<std::array<world::meta::type_id_t, N>, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const std::array<world::meta::type_id_t, N>& arr,
                FormatContext& ctx) const {
                    auto out = ctx.out();
                    *out++ = '[';
                    for (std::size_t i = 0; i < N; ++i) {
                        out = std::format_to(out, "{}", arr[i]);
                        if (i + 1 < N) out = std::format_to(out, ", ");
                    }
                    *out++ = ']';
                    return out;
                }
};

template <>
struct std::formatter<std::span<const world::meta::type_id_t>, char> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(std::span<const world::meta::type_id_t> s,
                FormatContext& ctx) const {
                    auto out = ctx.out();
                    *out++ = '[';
                    for (std::size_t i = 0; i < s.size(); ++i) {
                        out = std::format_to(out, "{}", s[i]);
                        if (i + 1 < s.size()) out = std::format_to(out, ", ");
                    }
                    *out++ = ']';
                    return out;
                }
};

#endif
