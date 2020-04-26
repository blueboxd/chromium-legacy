// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CHECKED_ITERATORS_H_
#define BASE_CONTAINERS_CHECKED_ITERATORS_H_

#include <iterator>
#include <memory>
#include <type_traits>

#include "base/containers/util.h"
#include "base/logging.h"

namespace base {

#if defined(_LIBCPP_VERSION)
// SFINAE friendly type trait that only enables an overload if the passed in
// type is trivially copy assignable, so that it would be safe to implement
// std::copy as a memmove, as libc++ does for some iterator types.
namespace internal {
template <typename T>
using PointerIfIsTriviallyCopyAssignable = std::enable_if_t<
    std::is_trivially_copy_assignable<std::remove_const_t<T>>::value,
    T*>;
}  // namespace internal
#endif

template <typename T>
class CheckedContiguousIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::random_access_iterator_tag;

  // Required for converting constructor below.
  template <typename U>
  friend class CheckedContiguousIterator;

  constexpr CheckedContiguousIterator() = default;

#if defined(_LIBCPP_VERSION)
  // This constructor is required to be able to use a CheckedContiguousIterator
  // as the third argument for the optimized pointer based std::copy version in
  // libc++. Do not use this constructor otherwise.
  constexpr CheckedContiguousIterator(T* ptr)
      : start_(ptr), current_(ptr), end_(ptr) {}

  // Friend __unwrap_iter so that it can get unchecked access to the underlying
  // pointer.
  template <typename U>
  friend constexpr internal::PointerIfIsTriviallyCopyAssignable<U>
  __unwrap_iter(CheckedContiguousIterator<U> iter);
#endif

  constexpr CheckedContiguousIterator(T* start, const T* end)
      : CheckedContiguousIterator(start, start, end) {}
  constexpr CheckedContiguousIterator(const T* start, T* current, const T* end)
      : start_(start), current_(current), end_(end) {
    CHECK_LE(start, current);
    CHECK_LE(current, end);
  }
  constexpr CheckedContiguousIterator(const CheckedContiguousIterator& other) =
      default;

  // Converting constructor allowing conversions like CCI<T> to CCI<const T>,
  // but disallowing CCI<const T> to CCI<T> or CCI<Derived> to CCI<Base>, which
  // are unsafe. Furthermore, this is the same condition as used by the
  // converting constructors of std::span<T> and std::unique_ptr<T[]>.
  // See https://wg21.link/n4042 for details.
  template <
      typename U,
      std::enable_if_t<std::is_convertible<U (*)[], T (*)[]>::value>* = nullptr>
  constexpr CheckedContiguousIterator(const CheckedContiguousIterator<U>& other)
      : start_(other.start_), current_(other.current_), end_(other.end_) {
    // We explicitly don't delegate to the 3-argument constructor here. Its
    // CHECKs would be redundant, since we expect |other| to maintain its own
    // invariant. However, DCHECKs never hurt anybody. Presumably.
    DCHECK_LE(other.start_, other.current_);
    DCHECK_LE(other.current_, other.end_);
  }

  ~CheckedContiguousIterator() = default;

  constexpr CheckedContiguousIterator& operator=(
      const CheckedContiguousIterator& other) = default;

  friend constexpr bool operator==(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ == rhs.current_;
  }

  friend constexpr bool operator!=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ != rhs.current_;
  }

  friend constexpr bool operator<(const CheckedContiguousIterator& lhs,
                                  const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ < rhs.current_;
  }

  friend constexpr bool operator<=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ <= rhs.current_;
  }
  friend constexpr bool operator>(const CheckedContiguousIterator& lhs,
                                  const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ > rhs.current_;
  }

  friend constexpr bool operator>=(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ >= rhs.current_;
  }

  constexpr CheckedContiguousIterator& operator++() {
    CHECK_NE(current_, end_);
    ++current_;
    return *this;
  }

  constexpr CheckedContiguousIterator operator++(int) {
    CheckedContiguousIterator old = *this;
    ++*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator--() {
    CHECK_NE(current_, start_);
    --current_;
    return *this;
  }

  constexpr CheckedContiguousIterator operator--(int) {
    CheckedContiguousIterator old = *this;
    --*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator+=(difference_type rhs) {
    if (rhs > 0) {
      CHECK_LE(rhs, end_ - current_);
    } else {
      CHECK_LE(-rhs, current_ - start_);
    }
    current_ += rhs;
    return *this;
  }

  constexpr CheckedContiguousIterator operator+(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it += rhs;
    return it;
  }

  constexpr CheckedContiguousIterator& operator-=(difference_type rhs) {
    if (rhs < 0) {
      CHECK_LE(-rhs, end_ - current_);
    } else {
      CHECK_LE(rhs, current_ - start_);
    }
    current_ -= rhs;
    return *this;
  }

  constexpr CheckedContiguousIterator operator-(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it -= rhs;
    return it;
  }

  constexpr friend difference_type operator-(
      const CheckedContiguousIterator& lhs,
      const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ - rhs.current_;
  }

  constexpr reference operator*() const {
    CHECK_NE(current_, end_);
    return *current_;
  }

  constexpr pointer operator->() const {
    CHECK_NE(current_, end_);
    return current_;
  }

  constexpr reference operator[](difference_type rhs) const {
    CHECK_GE(rhs, 0);
    CHECK_LT(rhs, end_ - current_);
    return current_[rhs];
  }

  static bool IsRangeMoveSafe(const CheckedContiguousIterator& from_begin,
                              const CheckedContiguousIterator& from_end,
                              const CheckedContiguousIterator& to)
      WARN_UNUSED_RESULT {
    if (from_end < from_begin)
      return false;
    const auto from_begin_uintptr = get_uintptr(from_begin.current_);
    const auto from_end_uintptr = get_uintptr(from_end.current_);
    const auto to_begin_uintptr = get_uintptr(to.current_);
    const auto to_end_uintptr =
        get_uintptr((to + std::distance(from_begin, from_end)).current_);

    return to_begin_uintptr >= from_end_uintptr ||
           to_end_uintptr <= from_begin_uintptr;
  }

 private:
  constexpr void CheckComparable(const CheckedContiguousIterator& other) const {
    CHECK_EQ(start_, other.start_);
    CHECK_EQ(end_, other.end_);
  }

  const T* start_ = nullptr;
  T* current_ = nullptr;
  const T* end_ = nullptr;
};

template <typename T>
using CheckedContiguousConstIterator = CheckedContiguousIterator<const T>;

#if defined(_LIBCPP_VERSION)
// This is a non-portable overload of libc++'s __unwrap_iter. This allows
// treating a CheckedContiguousIterator as a raw pointer within STL algorithms
// enabling performance optimizations that wouldn't be possible otherwise.
template <typename T>
constexpr internal::PointerIfIsTriviallyCopyAssignable<T> __unwrap_iter(
    CheckedContiguousIterator<T> iter) {
  return iter.current_;
}
#endif

}  // namespace base

#endif  // BASE_CONTAINERS_CHECKED_ITERATORS_H_
