// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

#include <string>
#include <tuple>        // for std::ignore
#include <type_traits>  // for std::is_trivially_copyable
#include <utility>      // for std::swap

#include "testing/gtest/include/gtest/gtest.h"

// This helps when copying arrays/vectors of pointers.
static_assert(std::is_trivially_copyable<CheckedPtr<void>>::value,
              "CheckedPtr should be trivially copyable");
static_assert(std::is_trivially_copyable<CheckedPtr<int>>::value,
              "CheckedPtr should be trivially copyable");
static_assert(std::is_trivially_copyable<CheckedPtr<std::string>>::value,
              "CheckedPtr should be trivially copyable");

namespace {

struct MyStruct {
  explicit MyStruct(int x) : x(x) {}
  int x;
};

struct Derived : MyStruct {
  Derived(int x, int y) : MyStruct(x), y(y) {}
  int y;
};

TEST(CheckedPtr, NullStarDereference) {
  CheckedPtr<int> ptr;
  EXPECT_DEATH_IF_SUPPORTED(if (*ptr == 42) return, "");
}

TEST(CheckedPtr, NullArrowDereference) {
  CheckedPtr<MyStruct> ptr;
  EXPECT_DEATH_IF_SUPPORTED(if (ptr->x == 42) return, "");
}

TEST(CheckedPtr, NullExtractNoDereference) {
  CheckedPtr<int> ptr;
  int* raw = ptr;
  std::ignore = raw;
}

TEST(CheckedPtr, StarDereference) {
  int foo = 42;
  CheckedPtr<int> ptr = &foo;
  EXPECT_EQ(*ptr, 42);
}

TEST(CheckedPtr, ArrowDereference) {
  MyStruct foo(42);
  CheckedPtr<MyStruct> ptr = &foo;
  EXPECT_EQ(ptr->x, 42);
}

TEST(CheckedPtr, VoidPtr) {
  const char foo[] = {0, 0, 1, 0};
  CheckedPtr<const void> ptr = foo;
  EXPECT_EQ(*static_cast<const int32_t*>(ptr), 65536);
}

TEST(CheckedPtr, OperatorEQ) {
  int foo;
  CheckedPtr<int> ptr1;
  EXPECT_TRUE(ptr1 == ptr1);

  CheckedPtr<int> ptr2;
  EXPECT_TRUE(ptr1 == ptr2);

  CheckedPtr<int> ptr3 = &foo;
  EXPECT_TRUE(&foo == ptr3);
  EXPECT_TRUE(ptr3 == &foo);
  EXPECT_FALSE(ptr1 == ptr3);

  ptr1 = &foo;
  EXPECT_TRUE(ptr1 == ptr3);
  EXPECT_TRUE(ptr3 == ptr1);
}

TEST(CheckedPtr, OperatorNE) {
  int foo;
  CheckedPtr<int> ptr1;
  EXPECT_FALSE(ptr1 != ptr1);

  CheckedPtr<int> ptr2;
  EXPECT_FALSE(ptr1 != ptr2);

  CheckedPtr<int> ptr3 = &foo;
  EXPECT_FALSE(&foo != ptr3);
  EXPECT_FALSE(ptr3 != &foo);
  EXPECT_TRUE(ptr1 != ptr3);

  ptr1 = &foo;
  EXPECT_FALSE(ptr1 != ptr3);
  EXPECT_FALSE(ptr3 != ptr1);
}

TEST(CheckedPtr, Cast) {
  Derived derived_val(42, 1024);
  CheckedPtr<Derived> checked_derived_ptr = &derived_val;
  MyStruct* raw_base_ptr = checked_derived_ptr;
  EXPECT_EQ(raw_base_ptr->x, 42);

  Derived* raw_derived_ptr = static_cast<Derived*>(raw_base_ptr);
  EXPECT_EQ(raw_derived_ptr->x, 42);
  EXPECT_EQ(raw_derived_ptr->y, 1024);

  CheckedPtr<MyStruct> checked_base_ptr = raw_derived_ptr;
  EXPECT_EQ(checked_base_ptr->x, 42);

  CheckedPtr<Derived> checked_derived_ptr2 =
      static_cast<Derived*>(checked_base_ptr);
  EXPECT_EQ(checked_derived_ptr2->x, 42);
  EXPECT_EQ(checked_derived_ptr2->y, 1024);

  const Derived* raw_const_derived_ptr = checked_derived_ptr2;
  EXPECT_EQ(raw_const_derived_ptr->x, 42);
  EXPECT_EQ(raw_const_derived_ptr->y, 1024);

  CheckedPtr<const Derived> checked_const_derived_ptr = raw_const_derived_ptr;
  EXPECT_EQ(checked_const_derived_ptr->x, 42);
  EXPECT_EQ(checked_const_derived_ptr->y, 1024);

  void* raw_void_ptr = checked_derived_ptr;
  CheckedPtr<void> checked_void_ptr = raw_derived_ptr;
  CheckedPtr<Derived> checked_derived_ptr3 =
      static_cast<Derived*>(raw_void_ptr);
  CheckedPtr<Derived> checked_derived_ptr4 =
      static_cast<Derived*>(checked_void_ptr);
  EXPECT_EQ(checked_derived_ptr3->x, 42);
  EXPECT_EQ(checked_derived_ptr3->y, 1024);
  EXPECT_EQ(checked_derived_ptr4->x, 42);
  EXPECT_EQ(checked_derived_ptr4->y, 1024);
}

TEST(CheckedPtr, CustomSwap) {
  int foo1, foo2;
  CheckedPtr<int> ptr1(&foo1);
  CheckedPtr<int> ptr2(&foo2);
  using std::swap;
  swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &foo2);
  EXPECT_EQ(ptr2.get(), &foo1);
}

TEST(CheckedPtr, StdSwap) {
  int foo1, foo2;
  CheckedPtr<int> ptr1(&foo1);
  CheckedPtr<int> ptr2(&foo2);
  std::swap(ptr1, ptr2);
  EXPECT_EQ(ptr1.get(), &foo2);
  EXPECT_EQ(ptr2.get(), &foo1);
}

TEST(CheckedPtr, AdvanceIntArray) {
  int foo[] = {42, 43, 44, 45};
  CheckedPtr<int> ptr = foo;
  for (int i = 0; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  ptr = &foo[1];
  for (int i = 1; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
}

TEST(CheckedPtr, AdvanceString) {
  const char kChars[] = "Hello";
  std::string str = kChars;
  CheckedPtr<const char> ptr = str.c_str();
  for (size_t i = 0; i < str.size(); ++i, ++ptr) {
    ASSERT_EQ(*ptr, kChars[i]);
  }
}

}  // namespace
