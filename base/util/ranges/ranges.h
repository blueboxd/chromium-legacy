// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_RANGES_RANGES_H_
#define BASE_UTIL_RANGES_RANGES_H_

#include <type_traits>

#include "base/util/ranges/iterator.h"

namespace util {

namespace ranges {

// Implementation of C++20's std::ranges::iterator_t.
//
// Reference: https://wg21.link/ranges.syn#:~:text=iterator_t
template <typename Range>
using iterator_t = decltype(ranges::begin(std::declval<Range&>()));

}  // namespace ranges

}  // namespace util

#endif  // BASE_UTIL_RANGES_RANGES_H_
