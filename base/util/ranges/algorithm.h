// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_RANGES_ALGORITHM_H_
#define BASE_UTIL_RANGES_ALGORITHM_H_

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/util/ranges/functional.h"
#include "base/util/ranges/iterator.h"
#include "base/util/ranges/ranges.h"

namespace util {
namespace ranges {

namespace internal {

// Returns a transformed version of the unary predicate `pred` applying `proj`
// to its argument before invoking `pred` on it.
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj>
constexpr auto ProjectedUnaryPredicate(Pred& pred, Proj& proj) noexcept {
  return [&pred, &proj](auto&& arg) -> bool {
    return invoke(pred, invoke(proj, std::forward<decltype(arg)>(arg)));
  };
}

// Returns a transformed version of the binary predicate `pred` applying `proj1`
// and `proj2` to its arguments before invoking `pred` on them.
// Ensures that the return type of `invoke(pred, ...)` is convertible to bool.
template <typename Pred, typename Proj1, typename Proj2>
constexpr auto ProjectedBinaryPredicate(Pred& pred,
                                        Proj1& proj1,
                                        Proj2& proj2) noexcept {
  return [&pred, &proj1, &proj2](auto&& lhs, auto&& rhs) -> bool {
    return invoke(pred, invoke(proj1, std::forward<decltype(lhs)>(lhs)),
                  invoke(proj2, std::forward<decltype(rhs)>(rhs)));
  };
}

// This alias is used below to restrict iterator based APIs to types for which
// `iterator_category` and the pre-increment and post-increment operators are
// defined. This is required in situations where otherwise an undesired overload
// would be chosen, e.g. copy_if. In spirit this is similar to C++20's
// std::input_or_output_iterator, a concept that each iterator should satisfy.
template <typename Iter,
          typename = decltype(++std::declval<Iter&>()),
          typename = decltype(std::declval<Iter&>()++)>
using iterator_category_t =
    typename std::iterator_traits<Iter>::iterator_category;

// This alias is used below to restrict range based APIs to types for which
// `iterator_category_t` is defined for the underlying iterator. This is
// required in situations where otherwise an undesired overload would be chosen,
// e.g. transform. In spirit this is similar to C++20's std::ranges::range, a
// concept that each range should satisfy.
template <typename Range>
using range_category_t = iterator_category_t<iterator_t<Range>>;

}  // namespace internal

// [alg.nonmodifying] Non-modifying sequence operations
// Reference: https://wg21.link/alg.nonmodifying

// [alg.all.of] All of
// Reference: https://wg21.link/alg.all.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `false` for some iterator `i` in the range
// `[first, last)`, and `true` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.all.of#:~:text=ranges::all_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool all_of(InputIterator first,
                      InputIterator last,
                      Pred pred,
                      Proj proj = {}) {
  return std::all_of(first, last,
                     internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `false` for some iterator `i` in `range`, and
// `true` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.all.of#:~:text=ranges::all_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool all_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::all_of(ranges::begin(range), ranges::end(range),
                        std::move(pred), std::move(proj));
}

// [alg.any.of] Any of
// Reference: https://wg21.link/alg.any.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `true` if `E(i)` is `true` for some iterator `i` in the range
// `[first, last)`, and `false` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.any.of#:~:text=ranges::any_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool any_of(InputIterator first,
                      InputIterator last,
                      Pred pred,
                      Proj proj = {}) {
  return std::any_of(first, last,
                     internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `true` if `E(i)` is `true` for some iterator `i` in `range`, and
// `false` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.any.of#:~:text=ranges::any_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool any_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::any_of(ranges::begin(range), ranges::end(range),
                        std::move(pred), std::move(proj));
}

// [alg.none.of] None of
// Reference: https://wg21.link/alg.none.of

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `true` for some iterator `i` in the range
// `[first, last)`, and `true` otherwise.
//
// Complexity: At most `last - first` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.none.of#:~:text=ranges::none_of(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr bool none_of(InputIterator first,
                       InputIterator last,
                       Pred pred,
                       Proj proj = {}) {
  return std::none_of(first, last,
                      internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `invoke(pred, invoke(proj, *i))`.
//
// Returns: `false` if `E(i)` is `true` for some iterator `i` in `range`, and
// `true` otherwise.
//
// Complexity: At most `size(range)` applications of the predicate and any
// projection.
//
// Reference: https://wg21.link/alg.none.of#:~:text=ranges::none_of(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr bool none_of(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::none_of(ranges::begin(range), ranges::end(range),
                         std::move(pred), std::move(proj));
}

// [alg.foreach] For each
// Reference: https://wg21.link/alg.foreach

// Effects: Calls `invoke(f, invoke(proj, *i))` for every iterator `i` in the
// range `[first, last)`, starting from `first` and proceeding to `last - 1`.
//
// Returns: `f`
// Note: std::ranges::for_each(I first,...) returns a for_each_result, rather
// than an invocable. For simplicitly we match std::for_each's return type
// instead.
//
// Complexity: Applies `f` and `proj` exactly `last - first` times.
//
// Remarks: If `f` returns a result, the result is ignored.
//
// Reference: https://wg21.link/alg.foreach#:~:text=ranges::for_each(I
template <typename InputIterator,
          typename Fun,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto for_each(InputIterator first,
                        InputIterator last,
                        Fun f,
                        Proj proj = {}) {
  std::for_each(first, last, [&f, &proj](auto&& arg) {
    return invoke(f, invoke(proj, std::forward<decltype(arg)>(arg)));
  });

  return f;
}

// Effects: Calls `invoke(f, invoke(proj, *i))` for every iterator `i` in the
// range `range`, starting from `begin(range)` and proceeding to `end(range) -
// 1`.
//
// Returns: `f`
// Note: std::ranges::for_each(R&& r,...) returns a for_each_result, rather
// than an invocable. For simplicitly we match std::for_each's return type
// instead.
//
// Complexity: Applies `f` and `proj` exactly `size(range)` times.
//
// Remarks: If `f` returns a result, the result is ignored.
//
// Reference: https://wg21.link/alg.foreach#:~:text=ranges::for_each(R
template <typename Range,
          typename Fun,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto for_each(Range&& range, Fun f, Proj proj = {}) {
  return ranges::for_each(ranges::begin(range), ranges::end(range),
                          std::move(f), std::move(proj));
}

// [alg.find] Find
// Reference: https://wg21.link/alg.find

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find(I
template <typename InputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find(InputIterator first,
                    InputIterator last,
                    const T& value,
                    Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::find_if instead of std::find.
  return std::find_if(first, last, [&proj, &value](auto&& lhs) {
    return invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find(Range&& range, const T& value, Proj proj = {}) {
  return ranges::find(ranges::begin(range), ranges::end(range), value,
                      std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find_if(InputIterator first,
                       InputIterator last,
                       Pred pred,
                       Proj proj = {}) {
  return std::find_if(first, last,
                      internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find_if(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::find_if(ranges::begin(range), ranges::end(range),
                         std::move(pred), std::move(proj));
}

// Let `E(i)` be `bool(!invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in the range `[first, last)` for which `E(i)`
// is `true`. Returns `last` if no such iterator is found.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if_not(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto find_if_not(InputIterator first,
                           InputIterator last,
                           Pred pred,
                           Proj proj = {}) {
  return std::find_if_not(first, last,
                          internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(!invoke(pred, invoke(proj, *i)))`.
//
// Returns: The first iterator `i` in `range` for which `E(i)` is `true`.
// Returns `end(range)` if no such iterator is found.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.find#:~:text=ranges::find_if_not(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto find_if_not(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::find_if_not(ranges::begin(range), ranges::end(range),
                             std::move(pred), std::move(proj));
}

// [alg.find.end] Find end
// Reference: https://wg21.link/alg.find.end

// Let:
// - `E(i,n)` be `invoke(pred, invoke(proj1, *(i + n)),
//                             invoke(proj2, *(first2 + n)))`
//
// - `i` be `last1` if `[first2, last2)` is empty, or if
//   `(last2 - first2) > (last1 - first1)` is `true`, or if there is no iterator
//   in the range `[first1, last1 - (last2 - first2))` such that for every
//   non-negative integer `n < (last2 - first2)`, `E(i,n)` is `true`. Otherwise
//   `i` is the last such iterator in `[first1, last1 - (last2 - first2))`.
//
// Returns: `i`
// Note: std::ranges::find_end(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::find_end's return type instead.
//
// Complexity:
// At most `(last2 - first2) * (last1 - first1 - (last2 - first2) + 1)`
// applications of the corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.find.end#:~:text=ranges::find_end(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto find_end(ForwardIterator1 first1,
                        ForwardIterator1 last1,
                        ForwardIterator2 first2,
                        ForwardIterator2 last2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return std::find_end(first1, last1, first2, last2,
                       internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let:
// - `E(i,n)` be `invoke(pred, invoke(proj1, *(i + n)),
//                             invoke(proj2, *(first2 + n)))`
//
// - `i` be `end(range1)` if `range2` is empty, or if
//   `size(range2) > size(range1)` is `true`, or if there is no iterator in the
//   range `[begin(range1), end(range1) - size(range2))` such that for every
//   non-negative integer `n < size(range2)`, `E(i,n)` is `true`. Otherwise `i`
//   is the last such iterator in `[begin(range1), end(range1) - size(range2))`.
//
// Returns: `i`
// Note: std::ranges::find_end(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::find_end's return type instead.
//
// Complexity: At most `size(range2) * (size(range1) - size(range2) + 1)`
// applications of the corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.find.end#:~:text=ranges::find_end(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto find_end(Range1&& range1,
                        Range2&& range2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return ranges::find_end(ranges::begin(range1), ranges::end(range1),
                          ranges::begin(range2), ranges::end(range2),
                          std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.find.first.of] Find first
// Reference: https://wg21.link/alg.find.first.of

// Let `E(i,j)` be `bool(invoke(pred, invoke(proj1, *i), invoke(proj2, *j)))`.
//
// Effects: Finds an element that matches one of a set of values.
//
// Returns: The first iterator `i` in the range `[first1, last1)` such that for
// some iterator `j` in the range `[first2, last2)` `E(i,j)` holds. Returns
// `last1` if `[first2, last2)` is empty or if no such iterator is found.
//
// Complexity: At most `(last1 - first1) * (last2 - first2)` applications of the
// corresponding predicate and any projections.
//
// Reference:
// https://wg21.link/alg.find.first.of#:~:text=ranges::find_first_of(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto find_first_of(ForwardIterator1 first1,
                             ForwardIterator1 last1,
                             ForwardIterator2 first2,
                             ForwardIterator2 last2,
                             Pred pred = {},
                             Proj1 proj1 = {},
                             Proj2 proj2 = {}) {
  return std::find_first_of(
      first1, last1, first2, last2,
      internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(i,j)` be `bool(invoke(pred, invoke(proj1, *i), invoke(proj2, *j)))`.
//
// Effects: Finds an element that matches one of a set of values.
//
// Returns: The first iterator `i` in `range1` such that for some iterator `j`
// in `range2` `E(i,j)` holds. Returns `end(range1)` if `range2` is empty or if
// no such iterator is found.
//
// Complexity: At most `size(range1) * size(range2)` applications of the
// corresponding predicate and any projections.
//
// Reference:
// https://wg21.link/alg.find.first.of#:~:text=ranges::find_first_of(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto find_first_of(Range1&& range1,
                             Range2&& range2,
                             Pred pred = {},
                             Proj1 proj1 = {},
                             Proj2 proj2 = {}) {
  return ranges::find_first_of(
      ranges::begin(range1), ranges::end(range1), ranges::begin(range2),
      ranges::end(range2), std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.adjacent.find] Adjacent find
// Reference: https://wg21.link/alg.adjacent.find

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i), invoke(proj, *(i + 1))))`.
//
// Returns: The first iterator `i` such that both `i` and `i + 1` are in the
// range `[first, last)` for which `E(i)` holds. Returns `last` if no such
// iterator is found.
//
// Complexity: Exactly `min((i - first) + 1, (last - first) - 1)` applications
// of the corresponding predicate, where `i` is `adjacent_find`'s return value.
//
// Reference:
// https://wg21.link/alg.adjacent.find#:~:text=ranges::adjacent_find(I
template <typename ForwardIterator,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto adjacent_find(ForwardIterator first,
                             ForwardIterator last,
                             Pred pred = {},
                             Proj proj = {}) {
  return std::adjacent_find(
      first, last, internal::ProjectedBinaryPredicate(pred, proj, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i), invoke(proj, *(i + 1))))`.
//
// Returns: The first iterator `i` such that both `i` and `i + 1` are in the
// range `range` for which `E(i)` holds. Returns `end(range)` if no such
// iterator is found.
//
// Complexity: Exactly `min((i - begin(range)) + 1, size(range) - 1)`
// applications of the corresponding predicate, where `i` is `adjacent_find`'s
// return value.
//
// Reference:
// https://wg21.link/alg.adjacent.find#:~:text=ranges::adjacent_find(R
template <typename Range,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto adjacent_find(Range&& range, Pred pred = {}, Proj proj = {}) {
  return ranges::adjacent_find(ranges::begin(range), ranges::end(range),
                               std::move(pred), std::move(proj));
}

// [alg.count] Count
// Reference: https://wg21.link/alg.count

// Let `E(i)` be `invoke(proj, *i) == value`.
//
// Effects: Returns the number of iterators `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count(I
template <typename InputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto count(InputIterator first,
                     InputIterator last,
                     const T& value,
                     Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::count_if instead of std::count.
  return std::count_if(first, last, [&proj, &value](auto&& lhs) {
    return invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `invoke(proj, *i) == value`.
//
// Effects: Returns the number of iterators `i` in `range` for which `E(i)`
// holds.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto count(Range&& range, const T& value, Proj proj = {}) {
  return ranges::count(ranges::begin(range), ranges::end(range), value,
                       std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Returns the number of iterators `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count_if(I
template <typename InputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>>
constexpr auto count_if(InputIterator first,
                        InputIterator last,
                        Pred pred,
                        Proj proj = {}) {
  return std::count_if(first, last,
                       internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Returns the number of iterators `i` in `range` for which `E(i)`
// holds.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.count#:~:text=ranges::count_if(R
template <typename Range,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto count_if(Range&& range, Pred pred, Proj proj = {}) {
  return ranges::count_if(ranges::begin(range), ranges::end(range),
                          std::move(pred), std::move(proj));
}

// [mismatch] Mismatch
// Reference: https://wg21.link/mismatch

// Let `E(n)` be `!invoke(pred, invoke(proj1, *(first1 + n)),
//                              invoke(proj2, *(first2 + n)))`.
//
// Let `N` be `min(last1 - first1, last2 - first2)`.
//
// Returns: `{ first1 + n, first2 + n }`, where `n` is the smallest integer in
// `[0, N)` such that `E(n)` holds, or `N` if no such integer exists.
//
// Complexity: At most `N` applications of the corresponding predicate and any
// projections.
//
// Reference: https://wg21.link/mismatch#:~:text=ranges::mismatch(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto mismatch(ForwardIterator1 first1,
                        ForwardIterator1 last1,
                        ForwardIterator2 first2,
                        ForwardIterator2 last2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return std::mismatch(first1, last1, first2, last2,
                       internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(n)` be `!invoke(pred, invoke(proj1, *(begin(range1) + n)),
//                              invoke(proj2, *(begin(range2) + n)))`.
//
// Let `N` be `min(size(range1), size(range2))`.
//
// Returns: `{ begin(range1) + n, begin(range2) + n }`, where `n` is the
// smallest integer in `[0, N)` such that `E(n)` holds, or `N` if no such
// integer exists.
//
// Complexity: At most `N` applications of the corresponding predicate and any
// projections.
//
// Reference: https://wg21.link/mismatch#:~:text=ranges::mismatch(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto mismatch(Range1&& range1,
                        Range2&& range2,
                        Pred pred = {},
                        Proj1 proj1 = {},
                        Proj2 proj2 = {}) {
  return ranges::mismatch(ranges::begin(range1), ranges::end(range1),
                          ranges::begin(range2), ranges::end(range2),
                          std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.equal] Equal
// Reference: https://wg21.link/alg.equal

// Let `E(i)` be
//   `invoke(pred, invoke(proj1, *i), invoke(proj2, *(first2 + (i - first1))))`.
//
// Returns: If `last1 - first1 != last2 - first2`, return `false.` Otherwise
// return `true` if `E(i)` holds for every iterator `i` in the range `[first1,
// last1)`. Otherwise, returns `false`.
//
// Complexity: If the types of `first1`, `last1`, `first2`, and `last2` meet the
// `RandomAccessIterator` requirements and `last1 - first1 != last2 - first2`,
// then no applications of the corresponding predicate and each projection;
// otherwise, at most `min(last1 - first1, last2 - first2)` applications of the
// corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.equal#:~:text=ranges::equal(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr bool equal(ForwardIterator1 first1,
                     ForwardIterator1 last1,
                     ForwardIterator2 first2,
                     ForwardIterator2 last2,
                     Pred pred = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  return std::equal(first1, last1, first2, last2,
                    internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Let `E(i)` be
//   `invoke(pred, invoke(proj1, *i),
//                 invoke(proj2, *(begin(range2) + (i - begin(range1)))))`.
//
// Returns: If `size(range1) != size(range2)`, return `false.` Otherwise return
// `true` if `E(i)` holds for every iterator `i` in `range1`. Otherwise, returns
// `false`.
//
// Complexity: If the types of `begin(range1)`, `end(range1)`, `begin(range2)`,
// and `end(range2)` meet the `RandomAccessIterator` requirements and
// `size(range1) != size(range2)`, then no applications of the corresponding
// predicate and each projection;
// otherwise, at most `min(size(range1), size(range2))` applications of the
// corresponding predicate and any projections.
//
// Reference: https://wg21.link/alg.equal#:~:text=ranges::equal(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr bool equal(Range1&& range1,
                     Range2&& range2,
                     Pred pred = {},
                     Proj1 proj1 = {},
                     Proj2 proj2 = {}) {
  return ranges::equal(ranges::begin(range1), ranges::end(range1),
                       ranges::begin(range2), ranges::end(range2),
                       std::move(pred), std::move(proj1), std::move(proj2));
}

// [alg.is.permutation] Is permutation
// Reference: https://wg21.link/alg.is.permutation

// Returns: If `last1 - first1 != last2 - first2`, return `false`. Otherwise
// return `true` if there exists a permutation of the elements in the range
// `[first2, last2)`, bounded by `[pfirst, plast)`, such that
// `ranges::equal(first1, last1, pfirst, plast, pred, proj, proj)` returns
// `true`; otherwise, returns `false`.
//
// Complexity: No applications of the corresponding predicate if
// ForwardIterator1 and ForwardIterator2 meet the requirements of random access
// iterators and `last1 - first1 != last2 - first2`. Otherwise, exactly
// `last1 - first1` applications of the corresponding predicate and projections
// if `ranges::equal(first1, last1, first2, last2, pred, proj, proj)` would
// return true;
// otherwise, at worst `O(N^2)`, where `N` has the value `last1 - first1`.
//
// Note: While std::ranges::is_permutation supports different projections for
// the first and second range, this is currently not supported due to
// dispatching to std::is_permutation, which demands that `pred` is an
// equivalence relation.
// TODO(https://crbug.com/1071094): Consider supporing different projections in
// the future.
//
// Reference:
// https://wg21.link/alg.is.permutation#:~:text=ranges::is_permutation(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr bool is_permutation(ForwardIterator1 first1,
                              ForwardIterator1 last1,
                              ForwardIterator2 first2,
                              ForwardIterator2 last2,
                              Pred pred = {},
                              Proj proj = {}) {
  return std::is_permutation(
      first1, last1, first2, last2,
      internal::ProjectedBinaryPredicate(pred, proj, proj));
}

// Returns: If `size(range1) != size(range2)`, return `false`. Otherwise return
// `true` if there exists a permutation of the elements in `range2`, bounded by
// `[pbegin, pend)`, such that
// `ranges::equal(range1, [pbegin, pend), pred, proj, proj)` returns `true`;
// otherwise, returns `false`.
//
// Complexity: No applications of the corresponding predicate if Range1 and
// Range2 meet the requirements of random access ranges and
// `size(range1) != size(range2)`. Otherwise, exactly `size(range1)`
// applications of the corresponding predicate and projections if
// `ranges::equal(range1, range2, pred, proj, proj)` would return true;
// otherwise, at worst `O(N^2)`, where `N` has the value `size(range1)`.
//
// Note: While std::ranges::is_permutation supports different projections for
// the first and second range, this is currently not supported due to
// dispatching to std::is_permutation, which demands that `pred` is an
// equivalence relation.
// TODO(https://crbug.com/1071094): Consider supporing different projections in
// the future.
//
// Reference:
// https://wg21.link/alg.is.permutation#:~:text=ranges::is_permutation(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr bool is_permutation(Range1&& range1,
                              Range2&& range2,
                              Pred pred = {},
                              Proj proj = {}) {
  return ranges::is_permutation(ranges::begin(range1), ranges::end(range1),
                                ranges::begin(range2), ranges::end(range2),
                                std::move(pred), std::move(proj));
}

// [alg.search] Search
// Reference: https://wg21.link/alg.search

// Returns: `i`, where `i` is the first iterator in the range
// `[first1, last1 - (last2 - first2))` such that for every non-negative integer
// `n` less than `last2 - first2` the condition
// `bool(invoke(pred, invoke(proj1, *(i + n)), invoke(proj2, *(first2 + n))))`
// is `true`.
// Returns `last1` if no such iterator exists.
// Note: std::ranges::search(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search's return type instead.
//
// Complexity: At most `(last1 - first1) * (last2 - first2)` applications of the
// corresponding predicate and projections.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto search(ForwardIterator1 first1,
                      ForwardIterator1 last1,
                      ForwardIterator2 first2,
                      ForwardIterator2 last2,
                      Pred pred = {},
                      Proj1 proj1 = {},
                      Proj2 proj2 = {}) {
  return std::search(first1, last1, first2, last2,
                     internal::ProjectedBinaryPredicate(pred, proj1, proj2));
}

// Returns: `i`, where `i` is the first iterator in the range
// `[begin(range1), end(range1) - size(range2))` such that for every
// non-negative integer `n` less than `size(range2)` the condition
// `bool(invoke(pred, invoke(proj1, *(i + n)),
//                    invoke(proj2, *(begin(range2) + n))))` is `true`.
// Returns `end(range1)` if no such iterator exists.
// Note: std::ranges::search(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search's return type instead.
//
// Complexity: At most `size(range1) * size(range2)` applications of the
// corresponding predicate and projections.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search(R1
template <typename Range1,
          typename Range2,
          typename Pred = ranges::equal_to,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto search(Range1&& range1,
                      Range2&& range2,
                      Pred pred = {},
                      Proj1 proj1 = {},
                      Proj2 proj2 = {}) {
  return ranges::search(ranges::begin(range1), ranges::end(range1),
                        ranges::begin(range2), ranges::end(range2),
                        std::move(pred), std::move(proj1), std::move(proj2));
}

// Mandates: The type `Size` is convertible to an integral type.
//
// Returns: `i` where `i` is the first iterator in the range
// `[first, last - count)` such that for every non-negative integer `n` less
// than `count`, the following condition holds:
// `invoke(pred, invoke(proj, *(i + n)), value)`.
// Returns `last` if no such iterator is found.
// Note: std::ranges::search_n(I1 first1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search_n's return type instead.
//
// Complexity: At most `last - first` applications of the corresponding
// predicate and projection.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search_n(I
template <typename ForwardIterator,
          typename Size,
          typename T,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto search_n(ForwardIterator first,
                        ForwardIterator last,
                        Size count,
                        const T& value,
                        Pred pred = {},
                        Proj proj = {}) {
  // The second arg is guaranteed to be `value`, so we'll simply apply the
  // identity projection.
  identity value_proj;
  return std::search_n(
      first, last, count, value,
      internal::ProjectedBinaryPredicate(pred, proj, value_proj));
}

// Mandates: The type `Size` is convertible to an integral type.
//
// Returns: `i` where `i` is the first iterator in the range
// `[begin(range), end(range) - count)` such that for every non-negative integer
// `n` less than `count`, the following condition holds:
// `invoke(pred, invoke(proj, *(i + n)), value)`.
// Returns `end(arnge)` if no such iterator is found.
// Note: std::ranges::search_n(R1&& r1,...) returns a range, rather than an
// iterator. For simplicitly we match std::search_n's return type instead.
//
// Complexity: At most `size(range)` applications of the corresponding predicate
// and projection.
//
// Reference: https://wg21.link/alg.search#:~:text=ranges::search_n(R
template <typename Range,
          typename Size,
          typename T,
          typename Pred = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto search_n(Range&& range,
                        Size count,
                        const T& value,
                        Pred pred = {},
                        Proj proj = {}) {
  return ranges::search_n(ranges::begin(range), ranges::end(range), count,
                          value, std::move(pred), std::move(proj));
}

// [alg.modifying.operations] Mutating sequence operations
// Reference: https://wg21.link/alg.modifying.operations

// [alg.copy] Copy
// Reference: https://wg21.link/alg.copy

// Let N be `last - first`.
//
// Preconditions: `result` is not in the range `[first, last)`.
//
// Effects: Copies elements in the range `[first, last)` into the range
// `[result, result + N)` starting from `first` and proceeding to `last`. For
// each non-negative integer `n < N` , performs `*(result + n) = *(first + n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy(InputIterator first,
                    InputIterator last,
                    OutputIterator result) {
  return std::copy(first, last, result);
}

// Let N be `size(range)`.
//
// Preconditions: `result` is not in `range`.
//
// Effects: Copies elements in `range` into the range `[result, result + N)`
// starting from `begin(range)` and proceeding to `end(range)`. For each
// non-negative integer `n < N` , performs
// *(result + n) = *(begin(range) + n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy(Range&& range, OutputIterator result) {
  return ranges::copy(ranges::begin(range), ranges::end(range), result);
}

// Let `N` be `max(0, n)`.
//
// Mandates: The type `Size` is convertible to an integral type.
//
// Effects: For each non-negative integer `i < N`, performs
// `*(result + i) = *(first + i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_n
template <typename InputIterator,
          typename Size,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_n(InputIterator first, Size n, OutputIterator result) {
  return std::copy_n(first, n, result);
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`, and `N` be the number
// of iterators `i` in the range `[first, last)` for which the condition `E(i)`
// holds.
//
// Preconditions: The ranges `[first, last)` and
// `[result, result + (last - first))` do not overlap.
//
// Effects: Copies all of the elements referred to by the iterator `i` in the
// range `[first, last)` for which `E(i)` is true.
//
// Returns: `result + N`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_if(InputIterator first,
                       InputIterator last,
                       OutputIterator result,
                       Pred pred,
                       Proj proj = {}) {
  return std::copy_if(first, last, result,
                      internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`, and `N` be the number
// of iterators `i` in `range` for which the condition `E(i)` holds.
//
// Preconditions: `range`  and `[result, result + size(range))` do not overlap.
//
// Effects: Copies all of the elements referred to by the iterator `i` in
// `range` for which `E(i)` is true.
//
// Returns: `result + N`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_if(R
template <typename Range,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto copy_if(Range&& range,
                       OutputIterator result,
                       Pred pred,
                       Proj proj = {}) {
  return ranges::copy_if(ranges::begin(range), ranges::end(range), result,
                         std::move(pred), std::move(proj));
}

// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `(first, last]`.
//
// Effects: Copies elements in the range `[first, last)` into the range
// `[result - N, result)` starting from `last - 1` and proceeding to `first`.
// For each positive integer `n ≤ N`, performs `*(result - n) = *(last - n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_backward(I1
template <typename BidirectionalIterator1,
          typename BidirectionalIterator2,
          typename = internal::iterator_category_t<BidirectionalIterator1>,
          typename = internal::iterator_category_t<BidirectionalIterator2>>
constexpr auto copy_backward(BidirectionalIterator1 first,
                             BidirectionalIterator1 last,
                             BidirectionalIterator2 result) {
  return std::copy_backward(first, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in the range `(begin(range), end(range)]`.
//
// Effects: Copies elements in `range` into the range `[result - N, result)`
// starting from `end(range) - 1` and proceeding to `begin(range)`. For each
// positive integer `n ≤ N`, performs `*(result - n) = *(end(range) - n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.copy#:~:text=ranges::copy_backward(R
template <typename Range,
          typename BidirectionalIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto copy_backward(Range&& range, BidirectionalIterator result) {
  return ranges::copy_backward(ranges::begin(range), ranges::end(range),
                               result);
}

// [alg.move] Move
// Reference: https://wg21.link/alg.move

// Let `E(n)` be `std::move(*(first + n))`.
//
// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `[first, last)`.
//
// Effects: Moves elements in the range `[first, last)` into the range `[result,
// result + N)` starting from `first` and proceeding to `last`. For each
// non-negative integer `n < N`, performs `*(result + n) = E(n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move(I
template <typename InputIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto move(InputIterator first,
                    InputIterator last,
                    OutputIterator result) {
  return std::move(first, last, result);
}

// Let `E(n)` be `std::move(*(begin(range) + n))`.
//
// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in `range`.
//
// Effects: Moves elements in `range` into the range `[result, result + N)`
// starting from `begin(range)` and proceeding to `end(range)`. For each
// non-negative integer `n < N`, performs `*(result + n) = E(n)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto move(Range&& range, OutputIterator result) {
  return ranges::move(ranges::begin(range), ranges::end(range), result);
}

// Let `E(n)` be `std::move(*(last - n))`.
//
// Let `N` be `last - first`.
//
// Preconditions: `result` is not in the range `(first, last]`.
//
// Effects: Moves elements in the range `[first, last)` into the range
// `[result - N, result)` starting from `last - 1` and proceeding to `first`.
// For each positive integer `n ≤ N`, performs `*(result - n) = E(n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move_backward(I1
template <typename BidirectionalIterator1,
          typename BidirectionalIterator2,
          typename = internal::iterator_category_t<BidirectionalIterator1>,
          typename = internal::iterator_category_t<BidirectionalIterator2>>
constexpr auto move_backward(BidirectionalIterator1 first,
                             BidirectionalIterator1 last,
                             BidirectionalIterator2 result) {
  return std::move_backward(first, last, result);
}

// Let `E(n)` be `std::move(*(end(range) - n))`.
//
// Let `N` be `size(range)`.
//
// Preconditions: `result` is not in the range `(begin(range), end(range)]`.
//
// Effects: Moves elements in `range` into the range `[result - N, result)`
// starting from `end(range) - 1` and proceeding to `begin(range)`. For each
// positive integer `n ≤ N`, performs `*(result - n) = E(n)`.
//
// Returns: `result - N`
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.move#:~:text=ranges::move_backward(R
template <typename Range,
          typename BidirectionalIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto move_backward(Range&& range, BidirectionalIterator result) {
  return ranges::move_backward(ranges::begin(range), ranges::end(range),
                               result);
}

// [alg.swap] Swap
// Reference: https://wg21.link/alg.swap

// Let `M` be `min(last1 - first1, last2 - first2)`.
//
// Preconditions: The two ranges `[first1, last1)` and `[first2, last2)` do not
// overlap. `*(first1 + n)` is swappable with `*(first2 + n)`.
//
// Effects: For each non-negative integer `n < M` performs
// `swap(*(first1 + n), *(first2 + n))`
//
// Returns: `first2 + M`
//
// Complexity: Exactly `M` swaps.
//
// Reference: https://wg21.link/alg.swap#:~:text=ranges::swap_ranges(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>>
constexpr auto swap_ranges(ForwardIterator1 first1,
                           ForwardIterator1 last1,
                           ForwardIterator2 first2,
                           ForwardIterator2 last2) {
  // std::swap_ranges does not have a `last2` overload. Thus we need to
  // adjust `last1` to ensure to not read past `last2`.
  last1 = std::next(first1, std::min(std::distance(first1, last1),
                                     std::distance(first2, last2)));
  return std::swap_ranges(first1, last1, first2);
}

// Let `M` be `min(size(range1), size(range2))`.
//
// Preconditions: The two ranges `range1` and `range2` do not overlap.
// `*(begin(range1) + n)` is swappable with `*(begin(range2) + n)`.
//
// Effects: For each non-negative integer `n < M` performs
// `swap(*(begin(range1) + n), *(begin(range2) + n))`
//
// Returns: `begin(range2) + M`
//
// Complexity: Exactly `M` swaps.
//
// Reference: https://wg21.link/alg.swap#:~:text=ranges::swap_ranges(R1
template <typename Range1,
          typename Range2,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>>
constexpr auto swap_ranges(Range1&& range1, Range2&& range2) {
  return ranges::swap_ranges(ranges::begin(range1), ranges::end(range1),
                             ranges::begin(range2), ranges::end(range2));
}

// [alg.transform] Transform
// Reference: https://wg21.link/alg.transform

// Let `N` be `last1 - first1`,
// `E(i)` be `invoke(op, invoke(proj, *(first1 + (i - result))))`.
//
// Preconditions: `op` does not invalidate iterators or subranges, nor modify
// elements in the ranges `[first1, first1 + N]`, and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `op` and any projections.
//
// Remarks: result may be equal to `first1`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(I
template <typename InputIterator,
          typename OutputIterator,
          typename UnaryOperation,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<UnaryOperation&,
                                       projected<InputIterator, Proj>>>
constexpr auto transform(InputIterator first1,
                         InputIterator last1,
                         OutputIterator result,
                         UnaryOperation op,
                         Proj proj = {}) {
  return std::transform(first1, last1, result, [&op, &proj](auto&& arg) {
    return invoke(op, invoke(proj, std::forward<decltype(arg)>(arg)));
  });
}

// Let `N` be `size(range)`,
// `E(i)` be `invoke(op, invoke(proj, *(begin(range) + (i - result))))`.
//
// Preconditions: `op` does not invalidate iterators or subranges, nor modify
// elements in the ranges `[begin(range), end(range)]`, and
// `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `op` and any projections.
//
// Remarks: result may be equal to `begin(range)`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(R
template <typename Range,
          typename OutputIterator,
          typename UnaryOperation,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<UnaryOperation&,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto transform(Range&& range,
                         OutputIterator result,
                         UnaryOperation op,
                         Proj proj = {}) {
  return ranges::transform(ranges::begin(range), ranges::end(range), result,
                           std::move(op), std::move(proj));
}

// Let:
// `N` be `min(last1 - first1, last2 - first2)`,
// `E(i)` be `invoke(binary_op, invoke(proj1, *(first1 + (i - result))),
//                              invoke(proj2, *(first2 + (i - result))))`.
//
// Preconditions: `binary_op` does not invalidate iterators or subranges, nor
// modify elements in the ranges `[first1, first1 + N]`, `[first2, first2 + N]`,
// and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `binary_op`, and any projections.
//
// Remarks: `result` may be equal to `first1` or `first2`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(I1
template <typename ForwardIterator1,
          typename ForwardIterator2,
          typename OutputIterator,
          typename BinaryOperation,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::iterator_category_t<ForwardIterator1>,
          typename = internal::iterator_category_t<ForwardIterator2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<BinaryOperation&,
                                       projected<ForwardIterator1, Proj1>,
                                       projected<ForwardIterator2, Proj2>>>
constexpr auto transform(ForwardIterator1 first1,
                         ForwardIterator1 last1,
                         ForwardIterator2 first2,
                         ForwardIterator2 last2,
                         OutputIterator result,
                         BinaryOperation binary_op,
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  // std::transform does not have a `last2` overload. Thus we need to adjust
  // `last1` to ensure to not read past `last2`.
  last1 = std::next(first1, std::min(std::distance(first1, last1),
                                     std::distance(first2, last2)));
  return std::transform(first1, last1, first2, result,
                        [&binary_op, &proj1, &proj2](auto&& lhs, auto&& rhs) {
                          return invoke(
                              binary_op,
                              invoke(proj1, std::forward<decltype(lhs)>(lhs)),
                              invoke(proj2, std::forward<decltype(rhs)>(rhs)));
                        });
}

// Let:
// `N` be `min(size(range1), size(range2)`,
// `E(i)` be `invoke(binary_op, invoke(proj1, *(begin(range1) + (i - result))),
//                              invoke(proj2, *(begin(range2) + (i - result))))`
//
// Preconditions: `binary_op` does not invalidate iterators or subranges, nor
// modify elements in the ranges `[begin(range1), end(range1)]`,
// `[begin(range2), end(range2)]`, and `[result, result + N]`.
//
// Effects: Assigns through every iterator `i` in the range
// `[result, result + N)` a new corresponding value equal to `E(i)`.
//
// Returns: `result + N`
//
// Complexity: Exactly `N` applications of `binary_op`, and any projections.
//
// Remarks: `result` may be equal to `begin(range1)` or `begin(range2)`.
//
// Reference: https://wg21.link/alg.transform#:~:text=ranges::transform(R1
template <typename Range1,
          typename Range2,
          typename OutputIterator,
          typename BinaryOperation,
          typename Proj1 = identity,
          typename Proj2 = identity,
          typename = internal::range_category_t<Range1>,
          typename = internal::range_category_t<Range2>,
          typename = internal::iterator_category_t<OutputIterator>,
          typename = indirect_result_t<BinaryOperation&,
                                       projected<iterator_t<Range1>, Proj1>,
                                       projected<iterator_t<Range2>, Proj2>>>
constexpr auto transform(Range1&& range1,
                         Range2&& range2,
                         OutputIterator result,
                         BinaryOperation binary_op,
                         Proj1 proj1 = {},
                         Proj2 proj2 = {}) {
  return ranges::transform(ranges::begin(range1), ranges::end(range1),
                           ranges::begin(range2), ranges::end(range2), result,
                           std::move(binary_op), std::move(proj1),
                           std::move(proj2));
}

// [alg.replace] Replace
// Reference: https://wg21.link/alg.replace

// Let `E(i)` be `bool(invoke(proj, *i) == old_value)`.
//
// Mandates: `new_value` is writable  to `first`.
//
// Effects: Substitutes elements referred by the iterator `i` in the range
// `[first, last)` with `new_value`, when `E(i)` is true.
//
// Returns: `last`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace(I
template <typename ForwardIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto replace(ForwardIterator first,
                       ForwardIterator last,
                       const T& old_value,
                       const T& new_value,
                       Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::replace_if instead of std::replace.
  std::replace_if(
      first, last,
      [&proj, &old_value](auto&& lhs) {
        return invoke(proj, std::forward<decltype(lhs)>(lhs)) == old_value;
      },
      new_value);
  return last;
}

// Let `E(i)` be `bool(invoke(proj, *i) == old_value)`.
//
// Mandates: `new_value` is writable  to `begin(range)`.
//
// Effects: Substitutes elements referred by the iterator `i` in `range` with
// `new_value`, when `E(i)` is true.
//
// Returns: `end(range)`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto replace(Range&& range,
                       const T& old_value,
                       const T& new_value,
                       Proj proj = {}) {
  return ranges::replace(ranges::begin(range), ranges::end(range), old_value,
                         new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Mandates: `new_value` is writable  to `first`.
//
// Effects: Substitutes elements referred by the iterator `i` in the range
// `[first, last)` with `new_value`, when `E(i)` is true.
//
// Returns: `last`
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_if(I
template <typename ForwardIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto replace_if(ForwardIterator first,
                          ForwardIterator last,
                          Predicate pred,
                          const T& new_value,
                          Proj proj = {}) {
  std::replace_if(first, last, internal::ProjectedUnaryPredicate(pred, proj),
                  new_value);
  return last;
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Mandates: `new_value` is writable  to `begin(range)`.
//
// Effects: Substitutes elements referred by the iterator `i` in `range` with
// `new_value`, when `E(i)` is true.
//
// Returns: `end(range)`
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_if(R
template <typename Range,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto replace_if(Range&& range,
                          Predicate pred,
                          const T& new_value,
                          Proj proj = {}) {
  return ranges::replace_if(ranges::begin(range), ranges::end(range),
                            std::move(pred), new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(proj, *(first + (i - result))) == old_value)`.
//
// Mandates: The results of the expressions `*first` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// (last - first))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(first + (i - result))` otherwise.
//
// Returns: `result + (last - first)`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy(InputIterator first,
                            InputIterator last,
                            OutputIterator result,
                            const T& old_value,
                            const T& new_value,
                            Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::replace_copy_if instead of std::replace_copy.
  std::replace_copy_if(
      first, last, result,
      [&proj, &old_value](auto&& lhs) {
        return invoke(proj, std::forward<decltype(lhs)>(lhs)) == old_value;
      },
      new_value);
  return last;
}

// Let `E(i)` be
// `bool(invoke(proj, *(begin(range) + (i - result))) == old_value)`.
//
// Mandates: The results of the expressions `*begin(range)` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// size(range))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(begin(range) + (i - result))` otherwise.
//
// Returns: `result + size(range)`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy(R
template <typename Range,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy(Range&& range,
                            OutputIterator result,
                            const T& old_value,
                            const T& new_value,
                            Proj proj = {}) {
  return ranges::replace_copy(ranges::begin(range), ranges::end(range), result,
                              old_value, new_value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *(first + (i - result)))))`.
//
// Mandates: The results of the expressions `*first` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// (last - first))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(first + (i - result))` otherwise.
//
// Returns: `result + (last - first)`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy_if(InputIterator first,
                               InputIterator last,
                               OutputIterator result,
                               Predicate pred,
                               const T& new_value,
                               Proj proj = {}) {
  return std::replace_copy_if(first, last, result,
                              internal::ProjectedUnaryPredicate(pred, proj),
                              new_value);
}

// Let `E(i)` be
// `bool(invoke(pred, invoke(proj, *(begin(range) + (i - result)))))`.
//
// Mandates: The results of the expressions `*begin(range)` and `new_value` are
// writable  to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Assigns through every iterator `i` in the range `[result, result +
// size(range))` a new corresponding value, `new_value` if `E(i)` is true, or
// `*(begin(range) + (i - result))` otherwise.
//
// Returns: `result + size(range)`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.replace#:~:text=ranges::replace_copy_if(R
template <typename Range,
          typename OutputIterator,
          typename Predicate,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto replace_copy_if(Range&& range,
                               OutputIterator result,
                               Predicate pred,
                               const T& new_value,
                               Proj proj = {}) {
  return ranges::replace_copy_if(ranges::begin(range), ranges::end(range),
                                 result, pred, new_value, std::move(proj));
}

// [alg.fill] Fill
// Reference: https://wg21.link/alg.fill

// Let `N` be `last - first`.
//
// Mandates: The expression `value` is writable to the output iterator.
//
// Effects: Assigns `value` through all the iterators in the range
// `[first, last)`.
//
// Returns: `last`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill(O
template <typename OutputIterator,
          typename T,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto fill(OutputIterator first, OutputIterator last, const T& value) {
  std::fill(first, last, value);
  return last;
}

// Let `N` be `size(range)`.
//
// Mandates: The expression `value` is writable to the output iterator.
//
// Effects: Assigns `value` through all the iterators in `range`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill(R
template <typename Range,
          typename T,
          typename = internal::range_category_t<Range>>
constexpr auto fill(Range&& range, const T& value) {
  return ranges::fill(ranges::begin(range), ranges::end(range), value);
}

// Let `N` be `max(0, n)`.
//
// Mandates: The expression `value` is writable to the output iterator.
// The type `Size` is convertible to an integral type.
//
// Effects: Assigns `value` through all the iterators in `[first, first + N)`.
//
// Returns: `first + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.fill#:~:text=ranges::fill_n(O
template <typename OutputIterator,
          typename Size,
          typename T,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto fill_n(OutputIterator first, Size n, const T& value) {
  return std::fill_n(first, n, value);
}

// [alg.generate] Generate
// Reference: https://wg21.link/alg.generate

// Let `N` be `last - first`.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in the range `[first, last)`.
//
// Returns: `last`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate(O
template <typename OutputIterator,
          typename Generator,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto generate(OutputIterator first,
                        OutputIterator last,
                        Generator gen) {
  std::generate(first, last, std::move(gen));
  return last;
}

// Let `N` be `size(range)`.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in `range`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate(R
template <typename Range,
          typename Generator,
          typename = internal::range_category_t<Range>>
constexpr auto generate(Range&& range, Generator gen) {
  return ranges::generate(ranges::begin(range), ranges::end(range),
                          std::move(gen));
}

// Let `N` be `max(0, n)`.
//
// Mandates: `Size` is convertible to an integral type.
//
// Effects: Assigns the result of successive evaluations of gen() through each
// iterator in the range `[first, first + N)`.
//
// Returns: `first + N`.
//
// Complexity: Exactly `N` evaluations of `gen()` and assignments.
//
// Reference: https://wg21.link/alg.generate#:~:text=ranges::generate_n(O
template <typename OutputIterator,
          typename Size,
          typename Generator,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto generate_n(OutputIterator first, Size n, Generator gen) {
  return std::generate_n(first, n, std::move(gen));
}

// [alg.remove] Remove
// Reference: https://wg21.link/alg.remove

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in the range
// `[first, last)` for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove(I
template <typename ForwardIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto remove(ForwardIterator first,
                      ForwardIterator last,
                      const T& value,
                      Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::remove_if instead of std::remove.
  return std::remove_if(first, last, [&proj, &value](auto&& lhs) {
    return invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in `range`
// for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto remove(Range&& range, const T& value, Proj proj = {}) {
  return ranges::remove(ranges::begin(range), ranges::end(range), value,
                        std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in the range
// `[first, last)` for which `E(i)` holds.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_if(I
template <typename ForwardIterator,
          typename Predicate,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto remove_if(ForwardIterator first,
                         ForwardIterator last,
                         Predicate pred,
                         Proj proj = {}) {
  return std::remove_if(first, last,
                        internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Effects: Eliminates all the elements referred to by iterator `i` in `range`.
//
// Returns: The end of the resulting range.
//
// Remarks: Stable.
//
// Complexity: Exactly `size(range)` applications of the corresponding predicate
// and any projection.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_if(R
template <typename Range,
          typename Predicate,
          typename Proj = identity,
          typename = internal::range_category_t<Range>>
constexpr auto remove_if(Range&& range, Predicate pred, Proj proj = {}) {
  return ranges::remove_if(ranges::begin(range), ranges::end(range),
                           std::move(pred), std::move(proj));
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Let `N` be the number of elements in `[first, last)` for which `E(i)` is
// false.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in the range
// `[first, last)` for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(I
template <typename InputIterator,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy(InputIterator first,
                           InputIterator last,
                           OutputIterator result,
                           const T& value,
                           Proj proj = {}) {
  // Note: In order to be able to apply `proj` to each element in [first, last)
  // we are dispatching to std::remove_copy_if instead of std::remove_copy.
  return std::remove_copy_if(first, last, result, [&proj, &value](auto&& lhs) {
    return invoke(proj, std::forward<decltype(lhs)>(lhs)) == value;
  });
}

// Let `E(i)` be `bool(invoke(proj, *i) == value)`.
//
// Let `N` be the number of elements in `range` for which `E(i)` is false.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in `range`
//  for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(R
template <typename Range,
          typename OutputIterator,
          typename T,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy(Range&& range,
                           OutputIterator result,
                           const T& value,
                           Proj proj = {}) {
  return ranges::remove_copy(ranges::begin(range), ranges::end(range), result,
                             value, std::move(proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Let `N` be the number of elements in `[first, last)` for which `E(i)` is
// false.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + (last -
// first))` do not overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in the range
// `[first, last)` for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy_if(I
template <typename InputIterator,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::iterator_category_t<InputIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy_if(InputIterator first,
                              InputIterator last,
                              OutputIterator result,
                              Pred pred,
                              Proj proj = {}) {
  return std::remove_copy_if(first, last, result,
                             internal::ProjectedUnaryPredicate(pred, proj));
}

// Let `E(i)` be `bool(invoke(pred, invoke(proj, *i)))`.
//
// Let `N` be the number of elements in `range` for which `E(i)` is false.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies all the elements referred to by the iterator `i` in `range`
//  for which `E(i)` is false.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range)` applications of the corresponding
// predicate and any projection.
//
// Remarks: Stable.
//
// Reference: https://wg21.link/alg.remove#:~:text=ranges::remove_copy(R
template <typename Range,
          typename OutputIterator,
          typename Pred,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto remove_copy_if(Range&& range,
                              OutputIterator result,
                              Pred pred,
                              Proj proj = {}) {
  return ranges::remove_copy_if(ranges::begin(range), ranges::end(range),
                                result, std::move(pred), std::move(proj));
}

// [alg.unique] Unique
// Reference: https://wg21.link/alg.unique

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *(i - 1)), invoke(proj, *i)))`.
//
// Effects: For a nonempty range, eliminates all but the first element from
// every consecutive group of equivalent elements referred to by the iterator
// `i` in the range `[first + 1, last)` for which `E(i)` is true.
//
// Returns: The end of the resulting range.
//
// Complexity: For nonempty ranges, exactly `(last - first) - 1` applications of
// the corresponding predicate and no more than twice as many applications of
// any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique(I
template <typename ForwardIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = indirect_result_t<Comp&,
                                       projected<ForwardIterator, Proj>,
                                       projected<ForwardIterator, Proj>>>
constexpr auto unique(ForwardIterator first,
                      ForwardIterator last,
                      Comp comp = {},
                      Proj proj = {}) {
  return std::unique(first, last,
                     internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *(i - 1)), invoke(proj, *i)))`.
//
// Effects: For a nonempty range, eliminates all but the first element from
// every consecutive group of equivalent elements referred to by the iterator
// `i` in the range `[begin(range) + 1, end(range))` for which `E(i)` is true.
//
// Returns: The end of the resulting range.
//
// Complexity: For nonempty ranges, exactly `size(range) - 1` applications of
// the corresponding predicate and no more than twice as many applications of
// any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique(R
template <typename Range,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = indirect_result_t<Comp&,
                                       projected<iterator_t<Range>, Proj>,
                                       projected<iterator_t<Range>, Proj>>>
constexpr auto unique(Range&& range, Comp comp = {}, Proj proj = {}) {
  return ranges::unique(ranges::begin(range), ranges::end(range),
                        std::move(comp), std::move(proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *i), invoke(proj, *(i - 1))))`.
//
// Mandates: `*first` is writable to `result`.
//
// Preconditions: The ranges `[first, last)` and
// `[result, result + (last - first))` do not overlap.
//
// Effects: Copies only the first element from every consecutive group of equal
// elements referred to by the iterator `i` in the range `[first, last)` for
// which `E(i)` holds.
//
// Returns: `result + N`.
//
// Complexity: Exactly `last - first - 1` applications of the corresponding
// predicate and no more than twice as many applications of any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique_copy(I
template <typename ForwardIterator,
          typename OutputIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto unique_copy(ForwardIterator first,
                           ForwardIterator last,
                           OutputIterator result,
                           Comp comp = {},
                           Proj proj = {}) {
  return std::unique_copy(first, last, result,
                          internal::ProjectedBinaryPredicate(comp, proj, proj));
}

// Let `E(i)` be `bool(invoke(comp, invoke(proj, *i), invoke(proj, *(i - 1))))`.
//
// Mandates: `*begin(range)` is writable to `result`.
//
// Preconditions: The ranges `range` and `[result, result + size(range))` do not
// overlap.
//
// Effects: Copies only the first element from every consecutive group of equal
// elements referred to by the iterator `i` in `range` for which `E(i)` holds.
//
// Returns: `result + N`.
//
// Complexity: Exactly `size(range) - 1` applications of the corresponding
// predicate and no more than twice as many applications of any projection.
//
// Reference: https://wg21.link/alg.unique#:~:text=ranges::unique_copy(R
template <typename Range,
          typename OutputIterator,
          typename Comp = ranges::equal_to,
          typename Proj = identity,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto unique_copy(Range&& range,
                           OutputIterator result,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::unique_copy(ranges::begin(range), ranges::end(range), result,
                             std::move(comp), std::move(proj));
}

// [alg.reverse] Reverse
// Reference: https://wg21.link/alg.reverse

// Effects: For each non-negative integer `i < (last - first) / 2`, applies
// `std::iter_swap` to all pairs of iterators `first + i, (last - i) - 1`.
//
// Returns: `last`.
//
// Complexity: Exactly `(last - first)/2` swaps.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse(I
template <typename BidirectionalIterator,
          typename = internal::iterator_category_t<BidirectionalIterator>>
constexpr auto reverse(BidirectionalIterator first,
                       BidirectionalIterator last) {
  std::reverse(first, last);
  return last;
}

// Effects: For each non-negative integer `i < size(range) / 2`, applies
// `std::iter_swap` to all pairs of iterators
// `begin(range) + i, (end(range) - i) - 1`.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `size(range)/2` swaps.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse(R
template <typename Range, typename = internal::range_category_t<Range>>
constexpr auto reverse(Range&& range) {
  return ranges::reverse(ranges::begin(range), ranges::end(range));
}

// Let `N` be `last - first`.
//
// Preconditions: The ranges `[first, last)` and `[result, result + N)` do not
// overlap.
//
// Effects: Copies the range `[first, last)` to the range `[result, result + N)`
// such that for every non-negative integer `i < N` the following assignment
// takes place: `*(result + N - 1 - i) = *(first + i)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse_copy(I
template <typename BidirectionalIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<BidirectionalIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto reverse_copy(BidirectionalIterator first,
                            BidirectionalIterator last,
                            OutputIterator result) {
  return std::reverse_copy(first, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: The ranges `range` and `[result, result + N)` do not
// overlap.
//
// Effects: Copies `range` to the range `[result, result + N)` such that for
// every non-negative integer `i < N` the following assignment takes place:
// `*(result + N - 1 - i) = *(begin(range) + i)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.reverse#:~:text=ranges::reverse_copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto reverse_copy(Range&& range, OutputIterator result) {
  return ranges::reverse_copy(ranges::begin(range), ranges::end(range), result);
}

// [alg.rotate] Rotate
// Reference: https://wg21.link/alg.rotate

// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges.
//
// Effects: For each non-negative integer `i < (last - first)`, places the
// element from the position `first + i` into position
// `first + (i + (last - middle)) % (last - first)`.
//
// Returns: `first + (last - middle)`.
//
// Complexity: At most `last - first` swaps.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate(I
template <typename ForwardIterator,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto rotate(ForwardIterator first,
                      ForwardIterator middle,
                      ForwardIterator last) {
  return std::rotate(first, middle, last);
}

// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges.
//
// Effects: For each non-negative integer `i < size(range)`, places the element
// from the position `begin(range) + i` into position
// `begin(range) + (i + (end(range) - middle)) % size(range)`.
//
// Returns: `begin(range) + (end(range) - middle)`.
//
// Complexity: At most `size(range)` swaps.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate(R
template <typename Range, typename = internal::range_category_t<Range>>
constexpr auto rotate(Range&& range, iterator_t<Range> middle) {
  return ranges::rotate(ranges::begin(range), middle, ranges::end(range));
}

// Let `N` be `last - first`.
//
// Preconditions: `[first, middle)` and `[middle, last)` are valid ranges. The
// ranges `[first, last)` and `[result, result + N)` do not overlap.
//
// Effects: Copies the range `[first, last)` to the range `[result, result + N)`
// such that for each non-negative integer `i < N` the following assignment
// takes place: `*(result + i) = *(first + (i + (middle - first)) % N)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate_copy(I
template <typename ForwardIterator,
          typename OutputIterator,
          typename = internal::iterator_category_t<ForwardIterator>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto rotate_copy(ForwardIterator first,
                           ForwardIterator middle,
                           ForwardIterator last,
                           OutputIterator result) {
  return std::rotate_copy(first, middle, last, result);
}

// Let `N` be `size(range)`.
//
// Preconditions: `[begin(range), middle)` and `[middle, end(range))` are valid
// ranges. The ranges `range` and `[result, result + N)` do not overlap.
//
// Effects: Copies `range` to the range `[result, result + N)` such that for
// each non-negative integer `i < N` the following assignment takes place:
// `*(result + i) = *(begin(range) + (i + (middle - begin(range))) % N)`.
//
// Returns: `result + N`.
//
// Complexity: Exactly `N` assignments.
//
// Reference: https://wg21.link/alg.rotate#:~:text=ranges::rotate_copy(R
template <typename Range,
          typename OutputIterator,
          typename = internal::range_category_t<Range>,
          typename = internal::iterator_category_t<OutputIterator>>
constexpr auto rotate_copy(Range&& range,
                           iterator_t<Range> middle,
                           OutputIterator result) {
  return ranges::rotate_copy(ranges::begin(range), middle, ranges::end(range),
                             result);
}

// [alg.random.sample] Sample
// Reference: https://wg21.link/alg.random.sample

// Currently not implemented due to lack of std::sample in C++14.
// TODO(crbug.com/1071094): Consider implementing a hand-rolled version.

// [alg.random.shuffle] Shuffle
// Reference: https://wg21.link/alg.random.shuffle

// Preconditions: The type `std::remove_reference_t<UniformRandomBitGenerator>`
// meets the uniform random bit generator requirements.
//
// Effects: Permutes the elements in the range `[first, last)` such that each
// possible permutation of those elements has equal probability of appearance.
//
// Returns: `last`.
//
// Complexity: Exactly `(last - first) - 1` swaps.
//
// Remarks: To the extent that the implementation of this function makes use of
// random numbers, the object referenced by g shall serve as the
// implementation's source of randomness.
//
// Reference: https://wg21.link/alg.random.shuffle#:~:text=ranges::shuffle(I
template <typename RandomAccessIterator,
          typename UniformRandomBitGenerator,
          typename = internal::iterator_category_t<RandomAccessIterator>>
constexpr auto shuffle(RandomAccessIterator first,
                       RandomAccessIterator last,
                       UniformRandomBitGenerator&& g) {
  std::shuffle(first, last, std::forward<UniformRandomBitGenerator>(g));
  return last;
}

// Preconditions: The type `std::remove_reference_t<UniformRandomBitGenerator>`
// meets the uniform random bit generator requirements.
//
// Effects: Permutes the elements in `range` such that each possible permutation
// of those elements has equal probability of appearance.
//
// Returns: `end(range)`.
//
// Complexity: Exactly `size(range) - 1` swaps.
//
// Remarks: To the extent that the implementation of this function makes use of
// random numbers, the object referenced by g shall serve as the
// implementation's source of randomness.
//
// Reference: https://wg21.link/alg.random.shuffle#:~:text=ranges::shuffle(R
template <typename Range,
          typename UniformRandomBitGenerator,
          typename = internal::range_category_t<Range>>
constexpr auto shuffle(Range&& range, UniformRandomBitGenerator&& g) {
  return ranges::shuffle(ranges::begin(range), ranges::end(range),
                         std::forward<UniformRandomBitGenerator>(g));
}

// [alg.nonmodifying] Sorting and related operations
// Reference: https://wg21.link/alg.sorting

// [alg.sort] Sorting
// Reference: https://wg21.link/alg.sort

// [sort] sort
// Reference: https://wg21.link/sort

// TODO(crbug.com/1071094): Implement.

// [stable.sort] stable_sort
// Reference: https://wg21.link/stable.sort

// TODO(crbug.com/1071094): Implement.

// [partial.sort] partial_sort
// Reference: https://wg21.link/partial.sort

// TODO(crbug.com/1071094): Implement.

// [partial.sort.copy] partial_sort_copy
// Reference: https://wg21.link/partial.sort.copy

// TODO(crbug.com/1071094): Implement.

// [is.sorted] is_sorted
// Reference: https://wg21.link/is.sorted

// TODO(crbug.com/1071094): Implement.

// [alg.nth.element] Nth element
// Reference: https://wg21.link/alg.nth.element

// TODO(crbug.com/1071094): Implement.

// [alg.binary.search] Binary search
// Reference: https://wg21.link/alg.binary.search

// [lower.bound] lower_bound
// Reference: https://wg21.link/lower.bound

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expression `bool(invoke(comp, invoke(proj, e), value))`.
//
// Returns: The furthermost iterator `i` in the range `[first, last]` such that
// for every iterator `j` in the range `[first, i)`,
// `bool(invoke(comp, invoke(proj, *j), value))` is true.
//
// Complexity: At most `log(last - first) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/lower.bound#:~:text=ranges::lower_bound(I
template <typename ForwardIterator,
          typename T,
          typename Proj = identity,
          typename Comp = ranges::less,
          typename = internal::iterator_category_t<ForwardIterator>>
constexpr auto lower_bound(ForwardIterator first,
                           ForwardIterator last,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  // The second arg is guaranteed to be `value`, so we'll simply apply the
  // identity projection.
  identity value_proj;
  return std::lower_bound(
      first, last, value,
      internal::ProjectedBinaryPredicate(comp, proj, value_proj));
}

// Preconditions: The elements `e` of `[first, last)` are partitioned with
// respect to the expression `bool(invoke(comp, invoke(proj, e), value))`.
//
// Returns: The furthermost iterator `i` in the range
// `[begin(range), end(range)]` such that for every iterator `j` in the range
// `[begin(range), i)`, `bool(invoke(comp, invoke(proj, *j), value))` is true.
//
// Complexity: At most `log(size(range)) + O(1)` comparisons and projections.
//
// Reference: https://wg21.link/lower.bound#:~:text=ranges::lower_bound(R
template <typename Range,
          typename T,
          typename Proj = identity,
          typename Comp = ranges::less>
constexpr auto lower_bound(Range&& range,
                           const T& value,
                           Comp comp = {},
                           Proj proj = {}) {
  return ranges::lower_bound(ranges::begin(range), ranges::end(range), value,
                             std::move(comp), std::move(proj));
}

// [upper.bound] upper_bound
// Reference: https://wg21.link/upper.bound

// TODO(crbug.com/1071094): Implement.

// [equal.range] equal_range
// Reference: https://wg21.link/equal.range

// TODO(crbug.com/1071094): Implement.

// [binary.search] binary_search
// Reference: https://wg21.link/binary.search

// TODO(crbug.com/1071094): Implement.

// [alg.partitions] Partitions
// Reference: https://wg21.link/alg.partitions

// TODO(crbug.com/1071094): Implement.

// [alg.merge] Merge
// Reference: https://wg21.link/alg.merge

// TODO(crbug.com/1071094): Implement.

// [alg.set.operations] Set operations on sorted structures
// Reference: https://wg21.link/alg.set.operations

// [includes] includes
// Reference: https://wg21.link/includes

// TODO(crbug.com/1071094): Implement.

// [set.union] set_union
// Reference: https://wg21.link/set.union

// TODO(crbug.com/1071094): Implement.

// [set.intersection] set_intersection
// Reference: https://wg21.link/set.intersection

// TODO(crbug.com/1071094): Implement.

// [set.difference] set_difference
// Reference: https://wg21.link/set.difference

// TODO(crbug.com/1071094): Implement.

// [set.symmetric.difference] set_symmetric_difference
// Reference: https://wg21.link/set.symmetric.difference

// TODO(crbug.com/1071094): Implement.

// [alg.heap.operations] Heap operations
// Reference: https://wg21.link/alg.heap.operations

// [push.heap] push_heap
// Reference: https://wg21.link/push.heap

// TODO(crbug.com/1071094): Implement.

// [pop.heap] pop_heap
// Reference: https://wg21.link/pop.heap

// TODO(crbug.com/1071094): Implement.

// [make.heap] make_heap
// Reference: https://wg21.link/make.heap

// TODO(crbug.com/1071094): Implement.

// [sort.heap] sort_heap
// Reference: https://wg21.link/sort.heap

// TODO(crbug.com/1071094): Implement.

// [is.heap] is_heap
// Reference: https://wg21.link/is.heap

// TODO(crbug.com/1071094): Implement.

// [alg.min.max] Minimum and maximum
// Reference: https://wg21.link/alg.min.max

// TODO(crbug.com/1071094): Implement.

// [alg.lex.comparison] Lexicographical comparison
// Reference: https://wg21.link/alg.lex.comparison

// TODO(crbug.com/1071094): Implement.

// [alg.permutation.generators] Permutation generators
// Reference: https://wg21.link/alg.permutation.generators

// TODO(crbug.com/1071094): Implement.

}  // namespace ranges

}  // namespace util

#endif  // BASE_UTIL_RANGES_ALGORITHM_H_
