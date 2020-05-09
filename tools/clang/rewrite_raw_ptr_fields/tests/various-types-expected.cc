// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>

#include "base/memory/checked_ptr.h"

namespace my_namespace {

class SomeClass {
 public:
  void Method(char) {}
  int data_member;
};

struct MyStruct {
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
};

}  // namespace my_namespace
