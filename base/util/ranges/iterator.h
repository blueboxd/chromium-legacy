// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_RANGES_ITERATOR_H_
#define BASE_UTIL_RANGES_ITERATOR_H_

#include <iterator>
#include <type_traits>

#include "base/util/ranges/functional.h"

namespace util {

// Simplified implementation of C++20's std::iter_reference_t.
// As opposed to std::iter_reference_t, this implementation does not restrict
// the type of `Iter`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=iter_reference_t
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Simplified implementation of C++20's std::indirect_result_t. As opposed to
// std::indirect_result_t, this implementation does not restrict the type of
// `Func` and `Iters`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=indirect_result_t
template <typename Func, typename... Iters>
using indirect_result_t = invoke_result_t<Func, iter_reference_t<Iters>...>;

// Simplified implementation of C++20's std::projected. As opposed to
// std::projected, this implementation does not explicitly restrict the type of
// `Iter` and `Proj`, but rather does so implicitly by requiring
// `indirect_result_t<Proj, Iter>` is a valid type. This is required for SFINAE
// friendliness.
//
// Reference: https://wg21.link/projected
template <typename Iter,
          typename Proj,
          typename IndirectResultT = indirect_result_t<Proj, Iter>>
struct projected {
  using value_type = std::remove_cv_t<std::remove_reference_t<IndirectResultT>>;

  IndirectResultT operator*() const;  // not defined
};

namespace ranges {

namespace internal {

using std::begin;
template <typename Range>
auto Begin(Range&& range) -> decltype(begin(std::forward<Range>(range))) {
  return begin(std::forward<Range>(range));
}

using std::end;
template <typename Range>
auto End(Range&& range) -> decltype(end(std::forward<Range>(range))) {
  return end(std::forward<Range>(range));
}

}  // namespace internal

// Simplified implementation of C++20's std::ranges::begin.
// As opposed to std::ranges::begin, this implementation does not prefer a
// member begin() over a free standing begin(), does not check whether begin()
// returns an iterator, does not inhibit ADL and is not constexpr.
//
// The trailing return type and dispatch to the internal implementation is
// necessary to be SFINAE friendly.
//
// Reference: https://wg21.link/range.access.begin
template <typename Range>
auto begin(Range&& range)
    -> decltype(internal::Begin(std::forward<Range>(range))) {
  return internal::Begin(std::forward<Range>(range));
}

// Simplified implementation of C++20's std::ranges::end.
// As opposed to std::ranges::end, this implementation does not prefer a
// member end() over a free standing end(), does not check whether end()
// returns an iterator, does not inhibit ADL and is not constexpr.
//
// The trailing return type and dispatch to the internal implementation is
// necessary to be SFINAE friendly.
//
// Reference: - https://wg21.link/range.access.end
template <typename Range>
auto end(Range&& range) -> decltype(internal::End(std::forward<Range>(range))) {
  return internal::End(std::forward<Range>(range));
}

}  // namespace ranges

}  // namespace util

#endif  // BASE_UTIL_RANGES_ITERATOR_H_
