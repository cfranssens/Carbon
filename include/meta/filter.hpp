// Filter for querying 
#include <type_traits>
#include <meta/type_list.hpp>

// Parser for querying, allowing queries to be defined within a single parameter pack. (static registration) 
namespace filter {
  template <typename... Ts> 
  struct With {};

  template <typename... Ts> 
  struct Without {}; 

  template<typename T>
  struct is_with : std::false_type {};

  template<typename... Ts>
  struct is_with<With<Ts...>> : std::true_type {};

  template<typename T>
  struct is_without : std::false_type {};

  template<typename... Ts>
  struct is_without<Without<Ts...>> : std::true_type {};

  template<typename QueryTypes, typename WithTypes, typename WithoutTypes> struct QueryLayout {
    using queries = QueryTypes;
    using withs = WithTypes;
    using withouts = WithoutTypes;
  };

  template<typename QueryList, typename WithList, typename WithoutList, typename... Remaining> struct Parse;
  template<typename QueryList, typename WithList, typename WithoutList>
  struct Parse<QueryList, WithList, WithoutList> {
    using type = QueryLayout<QueryList, WithList, WithoutList>;
  };

  template<typename... Qs, typename WithList, typename WithoutList, typename T, typename... Rest>
  struct Parse<meta::type_list<Qs...>, WithList, WithoutList, T, Rest...> {
    using type = typename Parse<meta::type_list<Qs..., T>, WithList, WithoutList, Rest...>::type;
  };

  template<typename... Qs, typename... Ws, typename WithoutList, typename... Added, typename... Rest>
  struct Parse<meta::type_list<Qs...>, meta::type_list<Ws...>, WithoutList, With<Added...>, Rest...> {
    using type = typename Parse<meta::type_list<Qs...>, meta::type_list<Ws..., Added...>, WithoutList, Rest...>::type;
  };

  template<typename... Qs, typename Withs, typename... WOs, typename... Added, typename... Rest>
  struct Parse<meta::type_list<Qs...>, Withs, meta::type_list<WOs...>, Without<Added...>, Rest...>{
    using type = typename Parse<meta::type_list<Qs...>, Withs, meta::type_list<WOs..., Added...>, Rest...>::type;
  };
}
