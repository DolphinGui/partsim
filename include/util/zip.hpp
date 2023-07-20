#pragma once

#include <concepts>
#include <fmt/core.h>
#include <iterator>
#include <limits>
#include <tuplet/tuple.hpp>
#include <type_traits>

namespace zip {
using tuplet::tuple;

namespace detail {

template <typename... Ts>
using first_type = std::tuple_element_t<0, std::tuple<Ts...>>;
template <typename... Ts>
constexpr size_t pack_len = std::tuple_size_v<std::tuple<Ts...>>;
template <typename... Ts> constexpr size_t tuplet_size(tuple<Ts...> t) {
  return t.N;
}

template <std::size_t Index>
constexpr void invoke_at(auto &&f, auto &&...tuples) {
  f(tuplet::get<Index>(std::forward<decltype(tuples)>(tuples))...);
}

template <std::size_t... Indices>
constexpr void apply_sequence(auto &&f, std::index_sequence<Indices...>,
                              auto &&...tuples) {
  (((void)invoke_at<Indices>(std::forward<decltype(f)>(f),
                             std::forward<decltype(tuples)>(tuples)...),
    ...));
}

template <typename... Ts>
constexpr void tuple_for(auto &&f, const tuple<Ts...> &a,
                         const tuple<Ts...> &b) {

  constexpr auto len = std::tuple_size_v<std::tuple<Ts...>>;

  apply_sequence(std::forward<decltype(f)>(f), std::make_index_sequence<len>{},
                 a, b);
}

template <typename... Ts>
concept post_inc_returnable =
    (!std::same_as<decltype(std::declval<Ts>()++), void> && ...);
} // namespace detail

template <typename... Its> struct Iterator {
  constexpr Iterator(Its &&...args)
    requires(std::weakly_incrementable<Its> && ...)
      : iterators{args...} {}

  constexpr Iterator &operator++() {
    auto iterate = [](auto &...args) { ((++args), ...); };
    apply(iterate, iterators);
    return *this;
  }

  constexpr Iterator operator++(int)
    requires detail::post_inc_returnable<Its...>
  {
    auto old = *this;
    auto iterate = [](auto &...args) { ((++args), ...); };
    apply(iterate, iterators);
    return old;
  }

  constexpr void operator++(int) {
    auto iterate = [](auto &...args) { ((++args), ...); };
    apply(iterate, iterators);
  }

  constexpr bool operator==(const Iterator &other) const {
    bool result = false;
    detail::tuple_for(
        [&](auto &&a, auto &&b) {
          if (a == b)
            result = true;
        },
        iterators, other.iterators);
    return result;
  }

  constexpr tuple<decltype(*std::declval<Its>())...> operator*() {
    // this feels stupid, but otherwise it doesn't deduce that it should
    // return a reference
    return iterators.map([](auto &&it) -> decltype(*it) { return *it; });
  }
  constexpr tuple<decltype(*std::declval<Its>())...> operator*() const {
    return iterators.map([](auto &&it) -> decltype(*it) { return *it; });
  }

  tuple<Its...> iterators;
};

// generic begin chooses either cbegin or begin depending on constness
template <typename Range> auto gbegin(Range &&r) { return r.begin(); }
template <typename Range> auto gbegin(const Range &&r) { return r.cbegin(); }
template <typename Range> auto gend(Range &&r) { return r.end(); }
template <typename Range> auto gend(const Range &&r) { return r.cend(); }

template <typename... Its> struct Range {
  template <typename... Ranges>
  constexpr Range(Ranges &&...ranges)
      : begins{gbegin(ranges)...}, ends{gend(ranges)...} {}

  constexpr auto begin() noexcept { return begins; }
  constexpr auto end() noexcept { return ends; }
  constexpr auto begin() const noexcept { return begins; }
  constexpr auto end() const noexcept { return ends; }

  constexpr size_t length() const noexcept {
    size_t result = std::numeric_limits<size_t>::max();

    detail::tuple_for<Its...>(
        [&](auto &&begin, auto &&end) {
          auto len = std::distance(begin, end);
          if (len < result) {
            result = len;
          }
        },
        begins.iterators, ends.iterators);

    return result;
  }

  constexpr auto operator[](size_t index)
    requires(std::random_access_iterator<Its> && ...)
  {
    return begins.iterators.map(
        [&](auto &&it) -> decltype(*it) { return *(it + index); });
  }

  Iterator<Its...> begins;
  Iterator<Its...> ends;
};

template <typename... Ranges>
Range(Ranges &&...ranges) -> Range<decltype(gbegin(ranges))...>;

template <typename... Ts> struct Test {
  template <typename... S> Test(S &&...args) {
    member = tuple{gbegin(args)...};
  }
  tuple<Ts...> member;
};

template <typename... Ranges>
Test(Ranges &&...ranges) -> Test<decltype(gbegin(ranges))...>;
} // namespace zip