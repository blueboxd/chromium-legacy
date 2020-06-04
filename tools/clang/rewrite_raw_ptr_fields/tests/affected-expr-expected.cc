// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>  // for uintptr_t

#include "base/memory/checked_ptr.h"
#include "gen/generated_header.h"

class SomeClass {};
class DerivedClass : public SomeClass {};

struct MyStruct {
  CheckedPtr<SomeClass> ptr;
  CheckedPtr<SomeClass> ptr2;
  CheckedPtr<const SomeClass> const_ptr;
  int (*func_ptr_field)();
};

namespace auto_tests {

MyStruct* GetMyStruct() {
  return nullptr;
}

SomeClass* GetSomeClass() {
  return nullptr;
}

SomeClass* ConvertSomeClassToSomeClass(SomeClass* some_class) {
  return some_class;
}

void foo() {
  MyStruct my_struct;

  // After the rewrite |my_struct.ptr_field| is no longer a pointer,
  // so |auto*| won't work.  We fix this up, by appending |.get()|.
  // Expected rewrite: auto* ptr_var = my_struct.ptr.get();
  auto* ptr_var = my_struct.ptr.get();

  // Tests for other kinds of initialization.
  // Expected rewrite: |.get()| should be appended in both cases below.
  auto* init_test1(my_struct.ptr.get());
  auto* init_test2{my_struct.ptr.get()};

  // Test for handling of the |const| qualifier.
  // Expected rewrite: const auto* ptr_var = my_struct.ptr.get();
  const auto* const_ptr_var = my_struct.ptr.get();

  // More complicated initialization expression, but the |ptr_field| struct
  // member dereference is still the top/last expression here.
  // Expected rewrite: ...->ptr.get()
  auto* complicated_var = GetMyStruct()->ptr.get();

  // The test below covers:
  // 1. Two variables with single |auto|,
  // 2. Tricky placement of |*| (next to the variable name).
  // Expected rewrite: ...ptr.get()... (twice in the 2nd example).
  auto *ptr_var1 = my_struct.ptr.get(), *ptr_var2 = GetSomeClass();
  auto *ptr_var3 = my_struct.ptr.get(), *ptr_var4 = my_struct.ptr.get();
  auto *ptr_var5 = GetSomeClass(), *ptr_var6 = my_struct.ptr.get();

  // Test for the case where
  // 1. The resulting type is the same as in the |ptr_var| and |complicated_var|
  //    examples
  // 2. Deep in the initialization expression there is a member dereference
  //    of |ptr_field|
  // but
  // 3. The final/top-level initialization expression doesn't dereference
  //    |ptr_field|.
  // No rewrite expected.
  auto* not_affected_field_var = ConvertSomeClassToSomeClass(my_struct.ptr);

  // Test for pointer |auto| assigned from non-CheckedPtr-elligible field.
  // No rewrite expected.
  auto* func_ptr_var = my_struct.func_ptr_field;

  // Test for non-pointer |auto| assigned from CheckedPtr-elligible field.
  // No rewrite expected.
  auto non_pointer_auto_var = my_struct.ptr;

  // Test for non-auto pointer.
  // No rewrite expected.
  SomeClass* non_auto_ptr_var = my_struct.ptr;
}

}  // namespace auto_tests

namespace printf_tests {

int ConvertSomeClassToInt(SomeClass* some_class) {
  return 123;
}

void MyPrintf(const char* fmt, ...) {}

void foo() {
  MyStruct s;

  // Expected rewrite: MyPrintf("%p", s.ptr.get());
  MyPrintf("%p", s.ptr.get());

  // Test - all arguments are rewritten.
  // Expected rewrite: MyPrintf("%p, %p", s.ptr.get(), s.ptr2.get());
  MyPrintf("%p, %p", s.ptr.get(), s.ptr2.get());

  // Test - only |s.ptr|-style arguments are rewritten.
  // Expected rewrite: MyPrintf("%d, %p", 123, s.ptr.get());
  MyPrintf("%d, %p", 123, s.ptr.get());

  // Test - |s.ptr| is deeply nested.
  // No rewrite expected.
  MyPrintf("%d", ConvertSomeClassToInt(s.ptr));
}

}  // namespace printf_tests

namespace cast_tests {

void foo() {
  MyStruct my_struct;

  // To get |const_cast<...>(...)| to compile after the rewrite we
  // need to rewrite the casted expression.
  // Expected rewrite: const_cast<SomeClass*>(my_struct.const_ptr.get());
  SomeClass* v = const_cast<SomeClass*>(my_struct.const_ptr.get());
  // Expected rewrite: const_cast<const SomeClass*>(my_struct.ptr.get());
  const SomeClass* v2 = const_cast<const SomeClass*>(my_struct.ptr.get());

  // To get |reinterpret_cast<uintptr_t>(...)| to compile after the rewrite we
  // need to rewrite the casted expression.
  // Expected rewrite: reinterpret_cast<uintptr_t>(my_struct.ptr.get());
  uintptr_t u = reinterpret_cast<uintptr_t>(my_struct.ptr.get());

  // There is no need to append |.get()| inside static_cast - unlike the
  // const_cast and reinterpret_cast examples above, static_cast will compile
  // just fine.
  DerivedClass* d = static_cast<DerivedClass*>(my_struct.ptr);
  void* void_var = static_cast<void*>(my_struct.ptr);
}

}  // namespace cast_tests

namespace ternary_operator_tests {

void foo(int x) {
  MyStruct my_struct;
  SomeClass* other_ptr = nullptr;

  // To avoid the following error type:
  //     conditional expression is ambiguous; 'const CheckedPtr<SomeClass>'
  //     can be converted to 'SomeClass *' and vice versa
  // we need to append |.get()| to |my_struct.ptr| below.
  //
  // Expected rewrite: ... my_struct.ptr.get() ...
  SomeClass* v = (x > 123) ? my_struct.ptr.get() : other_ptr;

  // Rewrite in the other position.
  // Expected rewrite: ... my_struct.ptr.get() ...
  SomeClass* v2 = (x > 456) ? other_ptr : my_struct.ptr.get();

  // No rewrite is needed for the first, conditional argument.
  // No rewrite expected.
  int v3 = my_struct.ptr ? 123 : 456;

  // Test for 1st and 2nd arg.  Only 2nd arg should be rewritten.
  SomeClass* v4 = my_struct.ptr ? my_struct.ptr.get() : other_ptr;
}

}  // namespace ternary_operator_tests

namespace generated_code_tests {

void MyPrintf(const char* fmt, ...) {}

void foo() {
  GeneratedStruct s;

  // No rewrite expected below (i.e. no |.get()| appended), because the field
  // dereferenced below comes from (simulated) generated code.
  MyPrintf("%p", s.ptr_field);
}

}  // namespace generated_code_tests
