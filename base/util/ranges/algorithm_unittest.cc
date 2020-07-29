// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/ranges/algorithm.h"

#include <algorithm>
#include <functional>
#include <random>
#include <utility>

#include "base/util/ranges/functional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Field;

namespace util {

namespace {

struct Int {
  constexpr Int(int value) : value(value) {}

  int value = 0;
};

bool operator==(Int lhs, Int rhs) {
  return lhs.value == rhs.value;
}

// Move-only int that clears `value` when moving out.
struct MoveOnlyInt {
  MoveOnlyInt(int value) : value(value) {}
  MoveOnlyInt(MoveOnlyInt&& other) : value(std::exchange(other.value, 0)) {}

  MoveOnlyInt& operator=(MoveOnlyInt&& other) {
    value = std::exchange(other.value, 0);
    return *this;
  }

  int value = 0;
};

constexpr bool is_even(int i) {
  return i % 2 == 0;
}

bool is_odd(int i) {
  return i % 2 == 1;
}

}  // namespace

TEST(RangesTest, AllOf) {
  // Note: Lambdas don't have a constexpr call operator prior to C++17, thus we
  // are providing our own anonyomous struct here.
  constexpr struct {
    constexpr bool operator()(int i) { return i != 0; }
  } is_non_zero;

  constexpr int array[] = {0, 1, 2, 3, 4, 5};

  static_assert(ranges::all_of(array + 1, array + 6, is_non_zero), "");
  static_assert(!ranges::all_of(array, is_non_zero), "");

  constexpr Int values[] = {0, 2, 4, 5};
  static_assert(
      ranges::all_of(values + 1, ranges::end(values), is_non_zero, &Int::value),
      "");
  static_assert(!ranges::all_of(values, is_non_zero, &Int::value), "");
}

TEST(RangesTest, AnyOf) {
  constexpr int array[] = {0, 1, 2, 3, 4, 5};

  static_assert(!ranges::any_of(array + 5, array + 6, is_even), "");
  static_assert(ranges::any_of(array, is_even), "");

  constexpr Int values[] = {{0}, {2}, {4}, {5}};
  static_assert(
      !ranges::any_of(values + 3, ranges::end(values), is_even, &Int::value),
      "");
  static_assert(ranges::any_of(values, is_even, &Int::value), "");
}

TEST(RangesTest, NoneOf) {
  // Note: Lambdas don't have a constexpr call operator prior to C++17, thus we
  // are providing our own anonyomous struct here.
  constexpr struct {
    constexpr bool operator()(int i) { return i == 0; }
  } is_zero;
  constexpr int array[] = {0, 1, 2, 3, 4, 5};

  static_assert(ranges::none_of(array + 1, array + 6, is_zero), "");
  static_assert(!ranges::none_of(array, is_zero), "");

  constexpr Int values[] = {{0}, {2}, {4}, {5}};
  static_assert(
      ranges::none_of(values + 1, ranges::end(values), is_zero, &Int::value),
      "");
  static_assert(!ranges::none_of(values, is_zero, &Int::value), "");
}

TEST(RangesTest, ForEach) {
  auto times_two = [](int& i) { i *= 2; };
  int array[] = {0, 1, 2, 3, 4, 5};

  auto result = ranges::for_each(array, array + 3, times_two);
  EXPECT_EQ(result.in, array + 3);
  EXPECT_EQ(result.fun, times_two);
  EXPECT_THAT(array, ElementsAre(0, 2, 4, 3, 4, 5));

  ranges::for_each(array + 3, array + 6, times_two);
  EXPECT_EQ(result.in, array + 3);
  EXPECT_EQ(result.fun, times_two);
  EXPECT_THAT(array, ElementsAre(0, 2, 4, 6, 8, 10));

  EXPECT_EQ(times_two, ranges::for_each(array, times_two).fun);
  EXPECT_THAT(array, ElementsAre(0, 4, 8, 12, 16, 20));

  Int values[] = {0, 2, 4, 5};
  EXPECT_EQ(times_two, ranges::for_each(values, times_two, &Int::value).fun);
  EXPECT_THAT(values,
              ElementsAre(Field(&Int::value, 0), Field(&Int::value, 4),
                          Field(&Int::value, 8), Field(&Int::value, 10)));
}

TEST(RangesTest, ForEachN) {
  auto times_two = [](int& i) { i *= 2; };
  int array[] = {0, 1, 2, 3, 4, 5};

  auto result = ranges::for_each_n(array, 3, times_two);
  EXPECT_EQ(result.in, array + 3);
  EXPECT_EQ(result.fun, times_two);
  EXPECT_THAT(array, ElementsAre(0, 2, 4, 3, 4, 5));

  Int values[] = {0, 2, 4, 5};
  EXPECT_EQ(times_two,
            ranges::for_each_n(values, 4, times_two, &Int::value).fun);
  EXPECT_THAT(values,
              ElementsAre(Field(&Int::value, 0), Field(&Int::value, 4),
                          Field(&Int::value, 8), Field(&Int::value, 10)));
}

TEST(RangesTest, Find) {
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_EQ(array + 6, ranges::find(array + 1, array + 6, 0));
  EXPECT_EQ(array, ranges::find(array, 0));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values, ranges::find(values, values, 0, &Int::value));
  EXPECT_EQ(ranges::end(values), ranges::find(values, 3, &Int::value));
}

TEST(RangesTest, FindIf) {
  auto is_at_least_5 = [](int i) { return i >= 5; };
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_EQ(array + 5, ranges::find_if(array, array + 5, is_at_least_5));
  EXPECT_EQ(array + 5, ranges::find_if(array, is_at_least_5));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 3,
            ranges::find_if(values, values + 3, is_odd, &Int::value));
  EXPECT_EQ(values + 3, ranges::find_if(values, is_odd, &Int::value));
}

TEST(RangesTest, FindIfNot) {
  auto is_less_than_5 = [](int i) { return i < 5; };
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_EQ(array + 5, ranges::find_if_not(array, array + 5, is_less_than_5));
  EXPECT_EQ(array + 5, ranges::find_if_not(array, is_less_than_5));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 3,
            ranges::find_if_not(values, values + 3, is_even, &Int::value));
  EXPECT_EQ(values + 3, ranges::find_if_not(values, is_even, &Int::value));
}

TEST(RangesTest, FindEnd) {
  int array1[] = {0, 1, 2};
  int array2[] = {4, 5, 6};
  int array3[] = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4,
                  0, 1, 2, 3, 0, 1, 2, 0, 1, 0};

  EXPECT_EQ(array3 + 15, ranges::find_end(array3, ranges::end(array3), array1,
                                          ranges::end(array1)));
  EXPECT_EQ(ranges::end(array3), ranges::find_end(array3, ranges::end(array3),
                                                  array2, ranges::end(array2)));
  EXPECT_EQ(array3 + 4,
            ranges::find_end(array3, ranges::end(array3), array2, array2 + 2));

  Int ints1[] = {{0}, {1}, {2}};
  Int ints2[] = {{4}, {5}, {6}};

  EXPECT_EQ(array3 + 15, ranges::find_end(array3, ints1, ranges::equal_to{},
                                          identity{}, &Int::value));
  EXPECT_EQ(ranges::end(array3),
            ranges::find_end(array3, ints2, ranges::equal_to{}, identity{},
                             &Int::value));
}

TEST(RangesTest, FindFirstOf) {
  int array1[] = {1, 2, 3};
  int array2[] = {7, 8, 9};
  int array3[] = {0, 1, 2, 3, 4, 5, 0, 1, 2, 3};

  EXPECT_EQ(array3 + 1, ranges::find_first_of(array3, ranges::end(array3),
                                              array1, ranges::end(array1)));
  EXPECT_EQ(ranges::end(array3),
            ranges::find_first_of(array3, ranges::end(array3), array2,
                                  ranges::end(array2)));
  Int ints1[] = {{1}, {2}, {3}};
  Int ints2[] = {{7}, {8}, {9}};

  EXPECT_EQ(array3 + 1, ranges::find_first_of(array3, ints1, ranges::equal_to{},
                                              identity{}, &Int::value));
  EXPECT_EQ(ranges::end(array3),
            ranges::find_first_of(array3, ints2, ranges::equal_to{}, identity{},
                                  &Int::value));
}

TEST(RangesTest, AdjacentFind) {
  int array[] = {1, 2, 3, 3};
  EXPECT_EQ(array + 2, ranges::adjacent_find(array, ranges::end(array)));
  EXPECT_EQ(array,
            ranges::adjacent_find(array, ranges::end(array), ranges::less{}));

  Int ints[] = {{6}, {6}, {5}, {4}};
  EXPECT_EQ(ints, ranges::adjacent_find(ints, ranges::equal_to{}, &Int::value));
  EXPECT_EQ(ranges::end(ints),
            ranges::adjacent_find(ints, ranges::less{}, &Int::value));
}

TEST(RangesTest, Count) {
  int array[] = {1, 2, 3, 3};
  EXPECT_EQ(1, ranges::count(array, array + 4, 1));
  EXPECT_EQ(1, ranges::count(array, array + 4, 2));
  EXPECT_EQ(1, ranges::count(array, array + 3, 3));
  EXPECT_EQ(2, ranges::count(array, array + 4, 3));

  Int ints[] = {{1}, {2}, {3}, {3}};
  EXPECT_EQ(1, ranges::count(ints, 1, &Int::value));
  EXPECT_EQ(1, ranges::count(ints, 2, &Int::value));
  EXPECT_EQ(2, ranges::count(ints, 3, &Int::value));
}

TEST(RangesTest, CountIf) {
  int array[] = {1, 2, 3, 3};
  EXPECT_EQ(0, ranges::count_if(array, array + 1, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 2, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 3, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 4, is_even));

  Int ints[] = {{1}, {2}, {3}, {3}};
  EXPECT_EQ(1, ranges::count_if(ints, is_even, &Int::value));
  EXPECT_EQ(3, ranges::count_if(ints, is_odd, &Int::value));
}

TEST(RangesTest, Mismatch) {
  int array1[] = {1, 3, 6, 7};
  int array2[] = {1, 3};
  int array3[] = {1, 3, 5, 7};
  EXPECT_EQ(std::make_pair(array1 + 2, array2 + 2),
            ranges::mismatch(array1, array1 + 4, array2, array2 + 2));
  EXPECT_EQ(std::make_pair(array1 + 2, array3 + 2),
            ranges::mismatch(array1, array1 + 4, array3, array3 + 4));

  EXPECT_EQ(std::make_pair(array1 + 2, array2 + 2),
            ranges::mismatch(array1, array2));
  EXPECT_EQ(std::make_pair(array1 + 2, array3 + 2),
            ranges::mismatch(array1, array3));
}

TEST(RangesTest, Equal) {
  int array1[] = {1, 3, 6, 7};
  int array2[] = {1, 3, 5, 7};
  EXPECT_TRUE(ranges::equal(array1, array1 + 2, array2, array2 + 2));
  EXPECT_FALSE(ranges::equal(array1, array1 + 4, array2, array2 + 4));
  EXPECT_FALSE(ranges::equal(array1, array1 + 2, array2, array2 + 3));

  Int ints[] = {{1}, {3}, {5}, {7}};
  EXPECT_TRUE(ranges::equal(ints, array2,
                            [](Int lhs, int rhs) { return lhs.value == rhs; }));
  EXPECT_TRUE(
      ranges::equal(array2, ints, ranges::equal_to{}, identity{}, &Int::value));
}

TEST(RangesTest, IsPermutation) {
  int array1[] = {1, 3, 6, 7};
  int array2[] = {7, 3, 1, 6};
  int array3[] = {1, 3, 5, 7};

  EXPECT_TRUE(ranges::is_permutation(array1, array1 + 4, array2, array2 + 4));
  EXPECT_FALSE(ranges::is_permutation(array1, array1 + 4, array3, array3 + 4));

  EXPECT_TRUE(ranges::is_permutation(array1, array2));
  EXPECT_FALSE(ranges::is_permutation(array1, array3));

  Int ints1[] = {{1}, {3}, {5}, {7}};
  Int ints2[] = {{1}, {5}, {3}, {7}};
  EXPECT_TRUE(ranges::is_permutation(
      ints1, ints2, [](Int lhs, Int rhs) { return lhs.value == rhs.value; }));

  EXPECT_TRUE(
      ranges::is_permutation(ints1, ints2, ranges::equal_to{}, &Int::value));

  EXPECT_FALSE(ranges::is_permutation(array1, ints2, ranges::equal_to{}, {},
                                      &Int::value));
  EXPECT_TRUE(ranges::is_permutation(array3, ints2, ranges::equal_to{}, {},
                                     &Int::value));
}

TEST(RangesTest, Search) {
  int array1[] = {0, 1, 2, 3};
  int array2[] = {0, 1, 5, 3};
  int array3[] = {0, 1, 2, 0, 1, 2, 3, 0, 1, 2, 3, 4};

  EXPECT_EQ(array3 + 3,
            ranges::search(array3, array3 + 12, array1, array1 + 4));
  EXPECT_EQ(array3 + 12,
            ranges::search(array3, array3 + 12, array2, array2 + 4));

  EXPECT_EQ(array3 + 3, ranges::search(array3, array1));
  EXPECT_EQ(array3 + 12, ranges::search(array3, array2));

  Int ints1[] = {{0}, {1}, {2}, {3}};
  Int ints2[] = {{0}, {1}, {5}, {3}};

  EXPECT_EQ(ints1 + 4, ranges::search(ints1, ints2, ranges::equal_to{},
                                      &Int::value, &Int::value));

  EXPECT_EQ(array3 + 3, ranges::search(array3, ints1, {}, {}, &Int::value));
  EXPECT_EQ(array3 + 12, ranges::search(array3, ints2, {}, {}, &Int::value));
}

TEST(RangesTest, SearchN) {
  int array[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(array, ranges::search_n(array, array + 6, 1, 0));
  EXPECT_EQ(array + 2, ranges::search_n(array, array + 6, 1, 1));
  EXPECT_EQ(array + 4, ranges::search_n(array, array + 6, 1, 2));
  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 1, 3));

  EXPECT_EQ(array, ranges::search_n(array, array + 6, 2, 0));
  EXPECT_EQ(array + 2, ranges::search_n(array, array + 6, 2, 1));
  EXPECT_EQ(array + 4, ranges::search_n(array, array + 6, 2, 2));
  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 2, 3));

  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 3, 0));
  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 3, 1));
  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 3, 2));
  EXPECT_EQ(array + 6, ranges::search_n(array, array + 6, 3, 3));

  Int ints[] = {{0}, {0}, {1}, {1}, {2}, {2}};
  EXPECT_EQ(ints, ranges::search_n(ints, 1, 0, {}, &Int::value));
  EXPECT_EQ(ints + 2, ranges::search_n(ints, 1, 1, {}, &Int::value));
  EXPECT_EQ(ints + 4, ranges::search_n(ints, 1, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::search_n(ints, 1, 3, {}, &Int::value));

  EXPECT_EQ(ints, ranges::search_n(ints, 2, 0, {}, &Int::value));
  EXPECT_EQ(ints + 2, ranges::search_n(ints, 2, 1, {}, &Int::value));
  EXPECT_EQ(ints + 4, ranges::search_n(ints, 2, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::search_n(ints, 2, 3, {}, &Int::value));

  EXPECT_EQ(ints + 6, ranges::search_n(ints, 3, 0, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::search_n(ints, 3, 1, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::search_n(ints, 3, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::search_n(ints, 3, 3, {}, &Int::value));
}

TEST(RangesTest, Copy) {
  int input[] = {1, 2, 3, 4, 5};
  int output[] = {6, 6, 6, 6, 6, 6, 6};
  auto equals_six = [](int i) { return i == 6; };

  EXPECT_EQ(output + 3, ranges::copy(input, input + 3, output));
  EXPECT_TRUE(std::equal(input, input + 3, output, output + 3));
  EXPECT_TRUE(std::all_of(output + 3, output + 7, equals_six));

  EXPECT_EQ(output + 5, ranges::copy(input, output));
  EXPECT_TRUE(std::equal(input, input + 5, output, output + 5));
  EXPECT_TRUE(std::all_of(output + 5, output + 7, equals_six));
}

TEST(RangesTest, CopyN) {
  int input[] = {1, 2, 3, 4, 5};
  int output[] = {6, 6, 6, 6, 6, 6, 6};
  auto equals_six = [](int i) { return i == 6; };

  EXPECT_EQ(output + 4, ranges::copy_n(input, 4, output));
  EXPECT_TRUE(std::equal(input, input + 4, output, output + 4));
  EXPECT_TRUE(std::all_of(output + 4, output + 7, equals_six));
}

TEST(RangesTest, CopyIf) {
  int input[] = {2, 4, 6, 8, 6};
  int output[] = {0, 0, 0, 0, 0, 0};
  auto equals_six = [](int i) { return i == 6; };
  auto equals_zero = [](int i) { return i == 0; };

  EXPECT_EQ(output + 1, ranges::copy_if(input, input + 4, output, equals_six));
  EXPECT_TRUE(std::all_of(output, output + 1, equals_six));
  EXPECT_TRUE(std::all_of(output + 1, output + 6, equals_zero));

  Int ints_in[] = {{2}, {4}, {6}, {8}, {6}};
  Int ints_out[] = {{0}, {0}, {0}, {0}, {0}, {0}};
  EXPECT_EQ(ints_out + 2,
            ranges::copy_if(ints_in, ints_out, equals_six, &Int::value));

  EXPECT_TRUE(ranges::all_of(ints_out, ints_out + 2, equals_six, &Int::value));
  EXPECT_TRUE(
      ranges::all_of(ints_out + 2, ints_out + 6, equals_zero, &Int::value));
}

TEST(RangesTest, CopyBackward) {
  int input[] = {2, 4, 6, 8, 6};
  int output[] = {0, 0, 0, 0, 0, 0};

  EXPECT_EQ(output + 1, ranges::copy_backward(input, input + 5, output + 6));
  EXPECT_THAT(output, ElementsAre(0, 2, 4, 6, 8, 6));

  Int ints_in[] = {{2}, {4}, {6}, {8}, {6}};
  Int ints_out[] = {{0}, {0}, {0}, {0}, {0}, {0}};

  EXPECT_EQ(ints_out, ranges::copy_backward(ints_in, ints_out + 5));
  EXPECT_TRUE(std::equal(ints_in, ints_in + 5, ints_out, ints_out + 5,
                         [](Int i, Int j) { return i.value == j.value; }));
}

TEST(RangesTest, Move) {
  MoveOnlyInt input[] = {6, 6, 6, 6, 6};
  MoveOnlyInt output[] = {0, 0, 0, 0, 0};
  auto equals_zero = [](const auto& i) { return i.value == 0; };
  auto equals_six = [](const auto& i) { return i.value == 6; };

  EXPECT_EQ(output + 3, ranges::move(input, input + 3, output));
  EXPECT_TRUE(std::all_of(input, input + 3, equals_zero));
  EXPECT_TRUE(std::all_of(input + 3, input + 5, equals_six));
  EXPECT_TRUE(std::all_of(output, output + 3, equals_six));
  EXPECT_TRUE(std::all_of(output + 3, output + 5, equals_zero));

  for (auto& in : input)
    in = 6;

  EXPECT_EQ(output + 5, ranges::move(input, output));
  EXPECT_TRUE(ranges::all_of(input, equals_zero));
  EXPECT_TRUE(ranges::all_of(output, equals_six));
}

TEST(RangesTest, MoveBackward) {
  MoveOnlyInt input[] = {6, 6, 6, 6, 6};
  MoveOnlyInt output[] = {0, 0, 0, 0, 0};
  auto equals_zero = [](const auto& i) { return i.value == 0; };
  auto equals_six = [](const auto& i) { return i.value == 6; };

  EXPECT_EQ(output + 2, ranges::move_backward(input, input + 3, output + 5));
  EXPECT_TRUE(std::all_of(input, input + 3, equals_zero));
  EXPECT_TRUE(std::all_of(input + 3, input + 5, equals_six));
  EXPECT_TRUE(std::all_of(output, output + 2, equals_zero));
  EXPECT_TRUE(std::all_of(output + 2, output + 5, equals_six));

  for (auto& in : input)
    in = 6;

  EXPECT_EQ(output, ranges::move_backward(input, output + 5));
  EXPECT_TRUE(ranges::all_of(input, equals_zero));
  EXPECT_TRUE(ranges::all_of(output, equals_six));
}

TEST(RangesTest, SwapRanges) {
  int ints1[] = {0, 0, 0, 0, 0};
  int ints2[] = {6, 6, 6, 6, 6};

  // Test that swap_ranges does not exceed `last2`.
  EXPECT_EQ(ints2 + 3, ranges::swap_ranges(ints1, ints1 + 5, ints2, ints2 + 3));
  EXPECT_THAT(ints1, ElementsAre(6, 6, 6, 0, 0));
  EXPECT_THAT(ints2, ElementsAre(0, 0, 0, 6, 6));

  // Test that swap_ranges does not exceed `last1`.
  EXPECT_EQ(ints2 + 3, ranges::swap_ranges(ints1, ints1 + 3, ints2, ints2 + 5));
  EXPECT_THAT(ints1, ElementsAre(0, 0, 0, 0, 0));
  EXPECT_THAT(ints2, ElementsAre(6, 6, 6, 6, 6));

  EXPECT_EQ(ints2 + 5,
            ranges::swap_ranges(ints1 + 3, ints1 + 5, ints2 + 3, ints2 + 5));
  EXPECT_THAT(ints1, ElementsAre(0, 0, 0, 6, 6));
  EXPECT_THAT(ints2, ElementsAre(6, 6, 6, 0, 0));

  EXPECT_EQ(ints2 + 5, ranges::swap_ranges(ints1, ints2));
  EXPECT_THAT(ints1, ElementsAre(6, 6, 6, 0, 0));
  EXPECT_THAT(ints2, ElementsAre(0, 0, 0, 6, 6));
}

TEST(RangesTest, UnaryTransform) {
  int input[] = {1, 2, 3, 4, 5};
  auto plus_1 = [](int i) { return i + 1; };
  auto times_2 = [](int i) { return i * 2; };

  EXPECT_EQ(input + 4,
            ranges::transform(input + 1, input + 4, input + 1, plus_1));
  EXPECT_THAT(input, ElementsAre(1, 3, 4, 5, 5));

  int output[] = {0, 0, 0, 0, 0};
  EXPECT_EQ(output + 3,
            ranges::transform(input + 1, input + 4, output, times_2));
  EXPECT_THAT(output, ElementsAre(6, 8, 10, 0, 0));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 4,
            ranges::transform(values, values, times_2, &Int::value));
  EXPECT_THAT(values, ElementsAre(Int{0}, Int{4}, Int{8}, Int{10}));
}

TEST(RangesTest, BinaryTransform) {
  int input[] = {1, 2, 3, 4, 5};
  int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 2, ranges::transform(input, input + 2, input + 3,
                                          input + 5, output, std::plus<>{}));
  EXPECT_THAT(output, ElementsAre(5, 7, 0, 0, 0));

  EXPECT_EQ(output + 5,
            ranges::transform(input, input, output, std::multiplies<>{}));
  EXPECT_THAT(output, ElementsAre(1, 4, 9, 16, 25));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 4,
            ranges::transform(values, values, values, std::minus<>{},
                              &Int::value, &Int::value));
  EXPECT_THAT(values, ElementsAre(Int{0}, Int{0}, Int{0}, Int{0}));
}

TEST(RangesTest, Replace) {
  int input[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(input + 2, ranges::replace(input, input + 2, 0, 2));
  EXPECT_THAT(input, ElementsAre(2, 2, 0, 0, 0));

  EXPECT_EQ(input + 5, ranges::replace(input, 0, 3));
  EXPECT_THAT(input, ElementsAre(2, 2, 3, 3, 3));
}

TEST(RangesTest, ReplaceIf) {
  int input[] = {0, 1, 2, 3, 4};

  EXPECT_EQ(input + 3, ranges::replace_if(input, input + 3, is_even, 9));
  EXPECT_THAT(input, ElementsAre(9, 1, 9, 3, 4));

  EXPECT_EQ(input + 5, ranges::replace_if(input, is_odd, 0));
  EXPECT_THAT(input, ElementsAre(0, 0, 0, 0, 4));

  Int ints[] = {0, 0, 1, 1, 0};
  EXPECT_EQ(ints + 5, ranges::replace_if(ints, is_odd, 3, &Int::value));
  EXPECT_THAT(ints, ElementsAre(0, 0, 3, 3, 0));
}

TEST(RangesTest, ReplaceCopy) {
  int input[] = {0, 0, 0, 0, 0};
  int output[] = {1, 1, 1, 1, 1};

  EXPECT_EQ(input + 2, ranges::replace_copy(input, input + 2, output, 0, 2));
  EXPECT_THAT(input, ElementsAre(0, 0, 0, 0, 0));
  EXPECT_THAT(output, ElementsAre(2, 2, 1, 1, 1));

  EXPECT_EQ(input + 5, ranges::replace_copy(input, output, 0, 3));
  EXPECT_THAT(input, ElementsAre(0, 0, 0, 0, 0));
  EXPECT_THAT(output, ElementsAre(3, 3, 3, 3, 3));
}

TEST(RangesTest, ReplaceCopyIf) {
  Int input[] = {0, 1, 2, 3, 4};
  Int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 3, ranges::replace_copy_if(input, input + 3, output,
                                                is_even, 9, &Int::value));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(9, 1, 9, 0, 0));

  EXPECT_EQ(output + 5,
            ranges::replace_copy_if(input, output, is_odd, 0, &Int::value));
  EXPECT_THAT(output, ElementsAre(0, 0, 2, 0, 4));
}

TEST(RangesTest, Fill) {
  int input[] = {1, 2, 3, 4, 5};

  EXPECT_EQ(input + 3, ranges::fill(input, input + 3, 0));
  EXPECT_THAT(input, ElementsAre(0, 0, 0, 4, 5));

  EXPECT_EQ(input + 5, ranges::fill(input, 1));
  EXPECT_THAT(input, ElementsAre(1, 1, 1, 1, 1));
}

TEST(RangesTest, FillN) {
  int input[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(input + 5, ranges::fill_n(input, 5, 5));
  EXPECT_THAT(input, ElementsAre(5, 5, 5, 5, 5));

  EXPECT_EQ(input + 3, ranges::fill_n(input, 3, 3));
  EXPECT_THAT(input, ElementsAre(3, 3, 3, 5, 5));
}

TEST(RangesTest, Generate) {
  int input[] = {0, 0, 0, 0, 0};

  auto gen = [count = 0]() mutable { return ++count; };
  EXPECT_EQ(input + 3, ranges::generate(input, input + 3, gen));
  EXPECT_THAT(input, ElementsAre(1, 2, 3, 0, 0));

  EXPECT_EQ(input + 5, ranges::generate(input, gen));
  EXPECT_THAT(input, ElementsAre(1, 2, 3, 4, 5));
}

TEST(RangesTest, GenerateN) {
  int input[] = {0, 0, 0, 0, 0};

  auto gen = [count = 0]() mutable { return ++count; };
  EXPECT_EQ(input + 4, ranges::generate_n(input, 4, gen));
  EXPECT_THAT(input, ElementsAre(1, 2, 3, 4, 0));
}

TEST(RangesTest, Remove) {
  int input[] = {1, 0, 1, 1, 0};

  EXPECT_EQ(input + 3, ranges::remove(input + 1, input + 5, 1));
  EXPECT_EQ(input[0], 1);
  EXPECT_EQ(input[1], 0);
  EXPECT_EQ(input[2], 0);

  Int ints[] = {2, 2, 1, 1, 2, 2};
  EXPECT_EQ(ints + 2, ranges::remove(ints, 2, &Int::value));
  EXPECT_EQ(ints[0], 1);
  EXPECT_EQ(ints[1], 1);
}

TEST(RangesTest, RemoveIf) {
  int input[] = {0, 1, 2, 3, 4};

  EXPECT_EQ(input + 2, ranges::remove_if(input, input + 4, is_even));
  EXPECT_EQ(input[0], 1);
  EXPECT_EQ(input[1], 3);
  EXPECT_EQ(input[4], 4);

  Int ints[] = {2, 2, 1, 1, 2, 2};
  EXPECT_EQ(ints + 2, ranges::remove_if(ints, is_even, &Int::value));
  EXPECT_EQ(ints[0], 1);
  EXPECT_EQ(ints[1], 1);
}

TEST(RangesTest, RemoveCopy) {
  int input[] = {0, 1, 2, 3, 4};
  int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 1, ranges::remove_copy(input, input + 2, output, 0));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(1, 0, 0, 0, 0));

  EXPECT_EQ(output + 4, ranges::remove_copy(input, output, 4));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(0, 1, 2, 3, 0));
}

TEST(RangesTest, RemovCopyIf) {
  Int input[] = {0, 1, 2, 3, 4};
  Int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 2, ranges::remove_copy_if(input, input + 4, output,
                                               is_even, &Int::value));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(1, 3, 0, 0, 0));

  EXPECT_EQ(output + 3,
            ranges::remove_copy_if(input, output, is_odd, &Int::value));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(0, 2, 4, 0, 0));
}

TEST(RangesTest, Unique) {
  int input[] = {0, 0, 1, 1, 2};

  EXPECT_EQ(input + 2, ranges::unique(input, input + 3));
  EXPECT_EQ(input[0], 0);
  EXPECT_EQ(input[1], 1);
  EXPECT_EQ(input[3], 1);
  EXPECT_EQ(input[4], 2);

  Int ints[] = {2, 2, 1, 1, 2, 2};
  EXPECT_EQ(ints + 3, ranges::unique(ints, {}, &Int::value));
  EXPECT_EQ(ints[0], 2);
  EXPECT_EQ(ints[1], 1);
  EXPECT_EQ(ints[2], 2);
}

TEST(RangesTest, UniqueCopy) {
  Int input[] = {0, 0, 1, 2, 2};
  Int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 3,
            ranges::unique_copy(input, input + 4, output, {}, &Int::value));
  EXPECT_THAT(input, ElementsAre(0, 0, 1, 2, 2));
  EXPECT_THAT(output, ElementsAre(0, 1, 2, 0, 0));

  EXPECT_EQ(output + 3, ranges::unique_copy(input, output, {}, &Int::value));
  EXPECT_THAT(input, ElementsAre(0, 0, 1, 2, 2));
  EXPECT_THAT(output, ElementsAre(0, 1, 2, 0, 0));
}

TEST(RangesTest, Reverse) {
  int input[] = {0, 1, 2, 3, 4};

  EXPECT_EQ(input + 4, ranges::reverse(input + 2, input + 4));
  EXPECT_THAT(input, ElementsAre(0, 1, 3, 2, 4));

  EXPECT_EQ(input + 5, ranges::reverse(input));
  EXPECT_THAT(input, ElementsAre(4, 2, 3, 1, 0));
}

TEST(RangesTest, ReverseCopy) {
  int input[] = {0, 1, 2, 3, 4};
  int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 2, ranges::reverse_copy(input + 2, input + 4, output));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(3, 2, 0, 0, 0));

  EXPECT_EQ(output + 5, ranges::reverse_copy(input, output));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(4, 3, 2, 1, 0));
}

TEST(RangesTest, Rotate) {
  int input[] = {0, 1, 2, 3, 4};

  EXPECT_EQ(input + 3, ranges::rotate(input + 2, input + 3, input + 4));
  EXPECT_THAT(input, ElementsAre(0, 1, 3, 2, 4));

  EXPECT_EQ(input + 3, ranges::rotate(input, input + 2));
  EXPECT_THAT(input, ElementsAre(3, 2, 4, 0, 1));
}

TEST(RangesTest, RotateCopy) {
  int input[] = {0, 1, 2, 3, 4};
  int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 2,
            ranges::rotate_copy(input + 2, input + 3, input + 4, output));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(3, 2, 0, 0, 0));

  EXPECT_EQ(output + 5, ranges::rotate_copy(input, input + 3, output));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));
  EXPECT_THAT(output, ElementsAre(3, 4, 0, 1, 2));
}

TEST(RangesTest, Shuffle) {
  int input[] = {0, 1, 2, 3, 4};

  // Shuffles input[2] and input[3], thus we can't be certain about their
  // positions.
  EXPECT_EQ(input + 4, ranges::shuffle(input + 2, input + 4,
                                       std::default_random_engine()));
  EXPECT_EQ(input[0], 0);
  EXPECT_EQ(input[1], 1);
  EXPECT_EQ(input[4], 4);
  EXPECT_THAT(input, ::testing::UnorderedElementsAre(0, 1, 2, 3, 4));

  EXPECT_EQ(input + 5, ranges::shuffle(input, std::default_random_engine()));
  EXPECT_THAT(input, ::testing::UnorderedElementsAre(0, 1, 2, 3, 4));
}

TEST(RangesTest, Sort) {
  int input[] = {3, 1, 2, 0, 4};
  EXPECT_EQ(input + 4, ranges::sort(input, input + 4));
  EXPECT_THAT(input, ElementsAre(0, 1, 2, 3, 4));

  EXPECT_EQ(input + 5, ranges::sort(input, input + 5, ranges::greater()));
  EXPECT_THAT(input, ElementsAre(4, 3, 2, 1, 0));

  Int ints[] = {6, 7, 9, 8, 5};
  EXPECT_EQ(ints + 5, ranges::sort(ints, {}, &Int::value));
  EXPECT_THAT(ints, ElementsAre(5, 6, 7, 8, 9));

  EXPECT_EQ(ints + 5, ranges::sort(ints, ranges::greater(), &Int::value));
  EXPECT_THAT(ints, ElementsAre(9, 8, 7, 6, 5));
}

TEST(RangesTest, StableSort) {
  // Integer divide each element by 2 to check stability of elements that
  // compare equal.
  auto idiv2 = [](int i) { return i / 2; };

  int input[] = {3, 1, 2, 0, 4};
  EXPECT_EQ(input + 4, ranges::stable_sort(input, input + 4, {}, idiv2));
  EXPECT_THAT(input, ElementsAre(1, 0, 3, 2, 4));

  EXPECT_EQ(input + 5,
            ranges::stable_sort(input, input + 5, ranges::greater()));
  EXPECT_THAT(input, ElementsAre(4, 3, 2, 1, 0));

  auto Idiv2 = [](Int i) { return i.value / 2; };
  Int ints[] = {6, 7, 9, 8, 5};
  EXPECT_EQ(ints + 5, ranges::stable_sort(ints, {}, Idiv2));
  EXPECT_THAT(ints, ElementsAre(5, 6, 7, 9, 8));

  EXPECT_EQ(ints + 5, ranges::stable_sort(ints, ranges::greater(), Idiv2));
  EXPECT_THAT(ints, ElementsAre(9, 8, 6, 7, 5));
}

TEST(RangesTest, PartialSort) {
  int input[] = {3, 1, 2, 0, 4};
  EXPECT_EQ(input + 4, ranges::partial_sort(input, input + 2, input + 4));
  EXPECT_EQ(input[0], 0);
  EXPECT_EQ(input[1], 1);

  EXPECT_EQ(input + 5, ranges::partial_sort(input, input + 3, input + 5,
                                            ranges::greater()));
  EXPECT_EQ(input[0], 4);
  EXPECT_EQ(input[1], 3);
  EXPECT_EQ(input[2], 2);

  Int ints[] = {6, 7, 9, 8, 5};
  EXPECT_EQ(ints + 5, ranges::partial_sort(ints, ints + 4, {}, &Int::value));
  EXPECT_EQ(ints[0], 5);
  EXPECT_EQ(ints[1], 6);
  EXPECT_EQ(ints[2], 7);
  EXPECT_EQ(ints[3], 8);

  EXPECT_EQ(ints + 5, ranges::partial_sort(ints, ints + 3, ranges::greater(),
                                           &Int::value));
  EXPECT_EQ(ints[0], 9);
  EXPECT_EQ(ints[1], 8);
  EXPECT_EQ(ints[2], 7);
}

TEST(RangesTest, PartialSortCopy) {
  int input[] = {3, 1, 2, 0, 4};
  int output[] = {0, 0, 0, 0, 0};
  EXPECT_EQ(output + 2,
            ranges::partial_sort_copy(input, input + 2, output, output + 4));
  EXPECT_THAT(input, ElementsAre(3, 1, 2, 0, 4));
  EXPECT_THAT(output, ElementsAre(1, 3, 0, 0, 0));

  EXPECT_EQ(output + 5,
            ranges::partial_sort_copy(input, input + 3, output + 3, output + 5,
                                      ranges::greater()));
  EXPECT_THAT(input, ElementsAre(3, 1, 2, 0, 4));
  EXPECT_THAT(output, ElementsAre(1, 3, 0, 3, 2));

  Int ints[] = {3, 1, 2, 0, 4};
  Int outs[] = {0, 0, 0};
  EXPECT_EQ(outs + 3, ranges::partial_sort_copy(ints, outs, {}, &Int::value,
                                                &Int::value));
  EXPECT_THAT(ints, ElementsAre(3, 1, 2, 0, 4));
  EXPECT_THAT(outs, ElementsAre(0, 1, 2));

  EXPECT_EQ(outs + 3, ranges::partial_sort_copy(ints, outs, ranges::greater(),
                                                &Int::value, &Int::value));
  EXPECT_THAT(ints, ElementsAre(3, 1, 2, 0, 4));
  EXPECT_THAT(outs, ElementsAre(4, 3, 2));

  EXPECT_EQ(outs + 3,
            ranges::partial_sort_copy(input, outs, {}, {}, &Int::value));
}

TEST(RangesTest, IsSorted) {
  int input[] = {3, 1, 2, 0, 4};
  EXPECT_TRUE(ranges::is_sorted(input + 1, input + 3));
  EXPECT_FALSE(ranges::is_sorted(input + 1, input + 4));
  EXPECT_TRUE(ranges::is_sorted(input, input + 2, ranges::greater()));

  Int ints[] = {0, 1, 2, 3, 4};
  EXPECT_TRUE(ranges::is_sorted(ints, {}, &Int::value));
  EXPECT_FALSE(ranges::is_sorted(ints, ranges::greater(), &Int::value));
}

TEST(RangesTest, IsSortedUntil) {
  int input[] = {3, 1, 2, 0, 4};
  EXPECT_EQ(input + 3, ranges::is_sorted_until(input + 1, input + 3));
  EXPECT_EQ(input + 3, ranges::is_sorted_until(input + 1, input + 4));
  EXPECT_EQ(input + 2,
            ranges::is_sorted_until(input, input + 2, ranges::greater()));

  Int ints[] = {0, 1, 2, 3, 4};
  EXPECT_EQ(ints + 5, ranges::is_sorted_until(ints, {}, &Int::value));
  EXPECT_EQ(ints + 1,
            ranges::is_sorted_until(ints, ranges::greater(), &Int::value));
}

TEST(RangesTest, LowerBound) {
  int array[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(array, ranges::lower_bound(array, array + 6, -1));
  EXPECT_EQ(array, ranges::lower_bound(array, array + 6, 0));
  EXPECT_EQ(array + 2, ranges::lower_bound(array, array + 6, 1));
  EXPECT_EQ(array + 4, ranges::lower_bound(array, array + 6, 2));
  EXPECT_EQ(array + 6, ranges::lower_bound(array, array + 6, 3));

  Int ints[] = {{0}, {0}, {1}, {1}, {2}, {2}};

  EXPECT_EQ(ints, ranges::lower_bound(ints, -1, {}, &Int::value));
  EXPECT_EQ(ints, ranges::lower_bound(ints, 0, {}, &Int::value));
  EXPECT_EQ(ints + 2, ranges::lower_bound(ints, 1, {}, &Int::value));
  EXPECT_EQ(ints + 4, ranges::lower_bound(ints, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::lower_bound(ints, 3, {}, &Int::value));

  const auto proj = [](const Int& i) { return 2 - i.value; };
  EXPECT_EQ(ints, ranges::lower_bound(ints, 3, ranges::greater{}, proj));
  EXPECT_EQ(ints, ranges::lower_bound(ints, 2, ranges::greater{}, proj));
  EXPECT_EQ(ints + 2, ranges::lower_bound(ints, 1, ranges::greater{}, proj));
  EXPECT_EQ(ints + 4, ranges::lower_bound(ints, 0, ranges::greater{}, proj));
  EXPECT_EQ(ints + 6, ranges::lower_bound(ints, -1, ranges::greater{}, proj));
}

}  // namespace util
