// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

#include <string>
#include <tuple>        // for std::ignore
#include <type_traits>  // for std::is_trivially_copyable
#include <utility>      // for std::swap

#include "testing/gtest/include/gtest/gtest.h"

static_assert(sizeof(CheckedPtr<void>) == sizeof(void*),
              "CheckedPtr shouldn't add memory overhead");
static_assert(sizeof(CheckedPtr<int>) == sizeof(int*),
              "CheckedPtr shouldn't add memory overhead");
static_assert(sizeof(CheckedPtr<std::string>) == sizeof(std::string*),
              "CheckedPtr shouldn't add memory overhead");

// This helps when copying arrays/vectors of pointers.
static_assert(std::is_trivially_copyable<CheckedPtr<void>>::value,
              "CheckedPtr should be trivially copyable");
static_assert(std::is_trivially_copyable<CheckedPtr<int>>::value,
              "CheckedPtr should be trivially copyable");
static_assert(std::is_trivially_copyable<CheckedPtr<std::string>>::value,
              "CheckedPtr should be trivially copyable");

namespace {

struct MyStruct {
  int x;
};

struct Base1 {
  explicit Base1(int b1) : b1(b1) {}
  int b1;
};

struct Base2 {
  explicit Base2(int b2) : b2(b2) {}
  int b2;
};

struct Derived : Base1, Base2 {
  Derived(int b1, int b2, int d) : Base1(b1), Base2(b2), d(d) {}
  int d;
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
  MyStruct foo = {42};
  CheckedPtr<MyStruct> ptr = &foo;
  EXPECT_EQ(ptr->x, 42);
}

TEST(CheckedPtr, ConstVoidPtr) {
  int32_t foo[] = {1234567890};
  CheckedPtr<const void> ptr = foo;
  EXPECT_EQ(*static_cast<const int32_t*>(ptr), 1234567890);
}

TEST(CheckedPtr, VoidPtr) {
  int32_t foo[] = {1234567890};
  CheckedPtr<void> ptr = foo;
  EXPECT_EQ(*static_cast<int32_t*>(ptr), 1234567890);
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

TEST(CheckedPtr, OperatorEQCast) {
  int foo = 42;
  CheckedPtr<int> int_ptr = &foo;
  CheckedPtr<void> void_ptr = &foo;
  EXPECT_TRUE(int_ptr == int_ptr);
  EXPECT_TRUE(void_ptr == void_ptr);
  EXPECT_TRUE(int_ptr == void_ptr);
  EXPECT_TRUE(void_ptr == int_ptr);

  Derived derived_val(42, 84, 1024);
  CheckedPtr<Derived> derived_ptr = &derived_val;
  CheckedPtr<Base1> base1_ptr = &derived_val;
  CheckedPtr<Base2> base2_ptr = &derived_val;
  EXPECT_TRUE(derived_ptr == derived_ptr);
  EXPECT_TRUE(derived_ptr == base1_ptr);
  EXPECT_TRUE(base1_ptr == derived_ptr);
  // |base2_ptr| points to the second base class of |derived|, so will be
  // located at an offset. While the stored raw uinptr_t values shouldn't match,
  // ensure that the internal pointer manipulation correctly offsets when
  // casting up and down the class hierarchy.
  EXPECT_NE(reinterpret_cast<uintptr_t>(base2_ptr.get()),
            reinterpret_cast<uintptr_t>(derived_ptr.get()));
  EXPECT_TRUE(derived_ptr == base2_ptr);
  EXPECT_TRUE(base2_ptr == derived_ptr);
}

TEST(CheckedPtr, OperatorNECast) {
  int foo = 42;
  CheckedPtr<int> int_ptr = &foo;
  CheckedPtr<void> void_ptr = &foo;
  EXPECT_FALSE(int_ptr != void_ptr);
  EXPECT_FALSE(void_ptr != int_ptr);

  Derived derived_val(42, 84, 1024);
  CheckedPtr<Derived> derived_ptr = &derived_val;
  CheckedPtr<Base1> base1_ptr = &derived_val;
  CheckedPtr<Base2> base2_ptr = &derived_val;
  EXPECT_FALSE(derived_ptr != base1_ptr);
  EXPECT_FALSE(base1_ptr != derived_ptr);
  // base2_ptr is pointing in the middle of derived_ptr, thus having a different
  // underlying address. Yet, they still should be equal.
  EXPECT_EQ(reinterpret_cast<uintptr_t>(base2_ptr.get()),
            reinterpret_cast<uintptr_t>(derived_ptr.get()) + 4);
  EXPECT_FALSE(derived_ptr != base2_ptr);
  EXPECT_FALSE(base2_ptr != derived_ptr);
}

TEST(CheckedPtr, Cast) {
  Derived derived_val(42, 84, 1024);
  CheckedPtr<Derived> checked_derived_ptr = &derived_val;
  Base1* raw_base1_ptr = checked_derived_ptr;
  EXPECT_EQ(raw_base1_ptr->b1, 42);
  Base2* raw_base2_ptr = checked_derived_ptr;
  EXPECT_EQ(raw_base2_ptr->b2, 84);

  Derived* raw_derived_ptr = static_cast<Derived*>(raw_base1_ptr);
  EXPECT_EQ(raw_derived_ptr->b1, 42);
  EXPECT_EQ(raw_derived_ptr->b2, 84);
  EXPECT_EQ(raw_derived_ptr->d, 1024);
  raw_derived_ptr = static_cast<Derived*>(raw_base2_ptr);
  EXPECT_EQ(raw_derived_ptr->b1, 42);
  EXPECT_EQ(raw_derived_ptr->b2, 84);
  EXPECT_EQ(raw_derived_ptr->d, 1024);

  CheckedPtr<Base1> checked_base1_ptr = raw_derived_ptr;
  EXPECT_EQ(checked_base1_ptr->b1, 42);
  CheckedPtr<Base2> checked_base2_ptr = raw_derived_ptr;
  EXPECT_EQ(checked_base2_ptr->b2, 84);

  CheckedPtr<Derived> checked_derived_ptr2 =
      static_cast<Derived*>(checked_base1_ptr);
  EXPECT_EQ(checked_derived_ptr2->b1, 42);
  EXPECT_EQ(checked_derived_ptr2->b2, 84);
  EXPECT_EQ(checked_derived_ptr2->d, 1024);
  checked_derived_ptr2 = static_cast<Derived*>(checked_base2_ptr);
  EXPECT_EQ(checked_derived_ptr2->b1, 42);
  EXPECT_EQ(checked_derived_ptr2->b2, 84);
  EXPECT_EQ(checked_derived_ptr2->d, 1024);

  const Derived* raw_const_derived_ptr = checked_derived_ptr2;
  EXPECT_EQ(raw_const_derived_ptr->b1, 42);
  EXPECT_EQ(raw_const_derived_ptr->b2, 84);
  EXPECT_EQ(raw_const_derived_ptr->d, 1024);

  CheckedPtr<const Derived> checked_const_derived_ptr = raw_const_derived_ptr;
  EXPECT_EQ(checked_const_derived_ptr->b1, 42);
  EXPECT_EQ(checked_const_derived_ptr->b2, 84);
  EXPECT_EQ(checked_const_derived_ptr->d, 1024);

  void* raw_void_ptr = checked_derived_ptr;
  CheckedPtr<void> checked_void_ptr = raw_derived_ptr;
  CheckedPtr<Derived> checked_derived_ptr3 =
      static_cast<Derived*>(raw_void_ptr);
  CheckedPtr<Derived> checked_derived_ptr4 =
      static_cast<Derived*>(checked_void_ptr);
  EXPECT_EQ(checked_derived_ptr3->b1, 42);
  EXPECT_EQ(checked_derived_ptr3->b2, 84);
  EXPECT_EQ(checked_derived_ptr3->d, 1024);
  EXPECT_EQ(checked_derived_ptr4->b1, 42);
  EXPECT_EQ(checked_derived_ptr4->b2, 84);
  EXPECT_EQ(checked_derived_ptr4->d, 1024);
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
  // operator++
  int foo[] = {42, 43, 44, 45};
  CheckedPtr<int> ptr = foo;
  for (int i = 0; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }
  ptr = &foo[1];
  for (int i = 1; i < 4; ++i, ++ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }

  // operator--
  ptr = &foo[3];
  for (int i = 3; i >= 0; --i, --ptr) {
    ASSERT_EQ(*ptr, 42 + i);
  }

  // operator+=
  ptr = foo;
  for (int i = 0; i < 4; i += 2, ptr += 2) {
    ASSERT_EQ(*ptr, 42 + i);
  }

  // operator-=
  ptr = &foo[3];
  for (int i = 3; i >= 0; i -= 2, ptr -= 2) {
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
