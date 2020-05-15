// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/memory/checked_ptr.h"

namespace my_namespace {

class SomeClass {
 public:
  void Method(char) {}
  int data_member;
};

// The class below deletes the |operator new| - this simulate's Blink's
// STACK_ALLOCATED macro and/or OilPan / GarbageCollected<T> classes.
class NoNewOperator {
  void* operator new(size_t) = delete;
};

struct MyStruct {
  // No rewrite expected for classes with no |operator new|.
  NoNewOperator* no_new_ptr;

  // Expected rewrite: CheckedPtr<CheckedPtr<SomeClass>> double_ptr;
  // TODO(lukasza): Handle recursion/nesting.
  CheckedPtr<SomeClass*> double_ptr;

  // Expected rewrite: CheckedPtr<void> void_ptr;
  CheckedPtr<void> void_ptr;

  // |bool*| used to be rewritten as |CheckedPtr<_Bool>| which doesn't compile:
  // use of undeclared identifier '_Bool'.
  //
  // Expected rewrite: CheckedPtr<bool> bool_ptr;
  CheckedPtr<bool> bool_ptr;
  // Expected rewrite: CheckedPtr<const bool> bool_ptr;
  CheckedPtr<const bool> const_bool_ptr;

  // Some types may be spelled in various, alternative ways.  If possible, the
  // rewriter should preserve the original spelling.
  //
  // Spelling of integer types.
  //
  // Expected rewrite: CheckedPtr<int> ...
  CheckedPtr<int> int_spelling1;
  // Expected rewrite: CheckedPtr<signed int> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: CheckedPtr<int> ...
  CheckedPtr<int> int_spelling2;
  // Expected rewrite: CheckedPtr<long int> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: CheckedPtr<long> ...
  CheckedPtr<long> int_spelling3;
  // Expected rewrite: CheckedPtr<unsigned> ...
  // TODO(lukasza): Fix?  Today this is rewritten into: CheckedPtr<unsigned int>
  CheckedPtr<unsigned int> int_spelling4;
  // Expected rewrite: CheckedPtr<int32_t> ...
  CheckedPtr<int32_t> int_spelling5;
  // Expected rewrite: CheckedPtr<int64_t> ...
  CheckedPtr<int64_t> int_spelling6;
  // Expected rewrite: CheckedPtr<int_fast32_t> ...
  CheckedPtr<int_fast32_t> int_spelling7;
  //
  // Spelling of structs and classes.
  //
  // Expected rewrite: CheckedPtr<SomeClass> ...
  CheckedPtr<SomeClass> class_spelling1;
  // Expected rewrite: CheckedPtr<class SomeClass> ...
  CheckedPtr<class SomeClass> class_spelling2;
  // Expected rewrite: CheckedPtr<my_namespace::SomeClass> ...
  CheckedPtr<my_namespace::SomeClass> class_spelling3;

  // No rewrite of function pointers expected, because they won't ever be either
  // A) allocated by PartitionAlloc or B) derived from CheckedPtrSupport.  In
  // theory |member_data_ptr| below can be A or B, but it can't be expressed as
  // non-pointer T used as a template argument of CheckedPtr.
  int (*func_ptr)();
  void (SomeClass::*member_func_ptr)(char);  // ~ pointer to SomeClass::Method
  int SomeClass::*member_data_ptr;  // ~ pointer to SomeClass::data_member
  typedef void (*func_ptr_typedef)(char);
  func_ptr_typedef func_ptr_typedef_field;

  // Typedef-ed or type-aliased pointees should participate in the rewriting. No
  // desugaring of the aliases is expected.
  typedef SomeClass SomeClassTypedef;
  using SomeClassAlias = SomeClass;
  typedef void (*func_ptr_typedef2)(char);
  // Expected rewrite: CheckedPtr<SomeClassTypedef> ...
  CheckedPtr<SomeClassTypedef> typedef_ptr;
  // Expected rewrite: CheckedPtr<SomeClassAlias> ...
  CheckedPtr<SomeClassAlias> alias_ptr;
  // Expected rewrite: CheckedPtr<func_ptr_typedef2> ...
  CheckedPtr<func_ptr_typedef2> ptr_to_function_ptr;

  // Typedefs and type alias definitions should not be rewritten.
  //
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  typedef SomeClass* SomeClassPtrTypedef;
  // No rewrite expected (for now - in V1 we only rewrite field decls).
  using SomeClassPtrAlias = SomeClass*;

  // Chromium is built with a warning/error that there are no user-defined
  // constructors invoked when initializing global-scoped values.
  // CheckedPtr<char> conversion might trigger a global constructor for string
  // literals:
  //     struct MyStruct {
  //       int foo;
  //       CheckedPtr<const char> bar;
  //     }
  //     MyStruct g_foo = {123, "string literal" /* global constr! */};
  // Because of the above, no rewrite is expected below.
  char* char_ptr;
  const char* const_char_ptr;
  wchar_t* wide_char_ptr;
  const wchar_t* const_wide_char_ptr;

  // |array_of_ptrs| is an array 123 of pointer to SomeClass.
  // No rewrite expected (this is not a pointer - this is an array).
  SomeClass* ptr_array[123];

  // |ptr_to_array| is a pointer to array 123 of const SomeClass.
  //
  // This test is based on EqualsFramesMatcher from
  // //net/websockets/websocket_channel_test.cc
  //
  // No rewrite expected (this *is* a pointer, but generating a correct
  // replacement is tricky, because the |replacement_range| needs to cover
  // "[123]" that comes *after* the field name).
  const SomeClass (*ptr_to_array)[123];

  // Definition of the non-freestanding struct should not disappear - i.e.
  // we do not want the rewrite to be: CheckedPtr<struct NonFreestandingStruct>.
  //
  // Expected rewrite: ??? (as long as the struct definition doesn't disappear).
  struct NonFreeStandingStruct {
    int non_ptr;
  } * ptr_to_non_free_standing_struct;

  // Pointer to an inline definition of a struct.  There is a risk of generating
  // an overlapping replacement (wrt the pointer field within the inline
  // struct).
  //
  // Note that before a fix, the rewriter would generate an overlapping
  // replacement under
  // //sandbox/linux/integration_tests/bpf_dsl_seccomp_unittest.cc
  // (see the ArgValue struct and the non-free-standing Tests struct inside).
  //
  // Expected rewrite: ??? (as long as there are no overlapping replacements).
  struct NonFreeStandingStruct2 {
    CheckedPtr<SomeClass> inner_ptr;
  } * ptr_to_non_free_standing_struct2;

  // Despite avoiding the problems in NonFreeStandingStruct and
  // NonFreeStandingStruct2 above, we should still rewrite the example below.
  struct FreeStandingStruct {
    // Expected rewrite: CheckedPtr<SomeClass> inner_ptr;
    CheckedPtr<SomeClass> inner_ptr;
  };
  // Expected rewrite: CheckedPtr<InnerStruct2> ...
  CheckedPtr<FreeStandingStruct> ptr_to_free_standing_struct;
};

}  // namespace my_namespace
