// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/util/ranges/algorithm.h"

#include <algorithm>
#include <utility>

#include "base/util/ranges/functional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Field;

namespace util {

namespace {

struct Int {
  int value = 0;
};

}  // namespace

TEST(RangesTest, AllOf) {
  auto is_non_zero = [](int i) { return i != 0; };
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_TRUE(ranges::all_of(array + 1, array + 6, is_non_zero));
  EXPECT_FALSE(ranges::all_of(array, is_non_zero));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_TRUE(ranges::all_of(values + 1, ranges::end(values), is_non_zero,
                             &Int::value));
  EXPECT_FALSE(ranges::all_of(values, is_non_zero, &Int::value));
}

TEST(RangesTest, AnyOf) {
  auto is_even = [](int i) { return i % 2 == 0; };
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_FALSE(ranges::any_of(array + 5, array + 6, is_even));
  EXPECT_TRUE(ranges::any_of(array, is_even));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_FALSE(
      ranges::any_of(values + 3, ranges::end(values), is_even, &Int::value));
  EXPECT_TRUE(ranges::any_of(values, is_even, &Int::value));
}

TEST(RangesTest, NoneOf) {
  auto is_zero = [](int i) { return i == 0; };
  int array[] = {0, 1, 2, 3, 4, 5};

  EXPECT_TRUE(ranges::none_of(array + 1, array + 6, is_zero));
  EXPECT_FALSE(ranges::none_of(array, is_zero));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_TRUE(
      ranges::none_of(values + 1, ranges::end(values), is_zero, &Int::value));
  EXPECT_FALSE(ranges::none_of(values, is_zero, &Int::value));
}

TEST(RangesTest, ForEach) {
  auto times_two = [](int& i) { i *= 2; };
  int array[] = {0, 1, 2, 3, 4, 5};

  ranges::for_each(array, array + 3, times_two);
  EXPECT_THAT(array, ElementsAre(0, 2, 4, 3, 4, 5));

  ranges::for_each(array + 3, array + 6, times_two);
  EXPECT_THAT(array, ElementsAre(0, 2, 4, 6, 8, 10));

  EXPECT_EQ(times_two, ranges::for_each(array, times_two));
  EXPECT_THAT(array, ElementsAre(0, 4, 8, 12, 16, 20));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(times_two, ranges::for_each(values, times_two, &Int::value));
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

  auto is_odd = [](int i) { return i % 2 == 1; };
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

  auto is_even = [](int i) { return i % 2 == 0; };
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
  auto is_even = [](int i) { return i % 2 == 0; };
  int array[] = {1, 2, 3, 3};
  EXPECT_EQ(0, ranges::count_if(array, array + 1, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 2, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 3, is_even));
  EXPECT_EQ(1, ranges::count_if(array, array + 4, is_even));

  auto is_odd = [](int i) { return i % 2 == 1; };
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

}  // namespace util
