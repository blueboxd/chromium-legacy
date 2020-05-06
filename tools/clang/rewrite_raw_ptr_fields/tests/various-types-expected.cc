// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/checked_ptr.h"

class SomeClass;

struct MyStruct {
  // Expected rewrite: CheckedPtr<CheckedPtr<SomeClass>> double_ptr;
  // TODO(lukasza): Handle recursion/nesting.
  CheckedPtr<SomeClass*> double_ptr;

  // Expected rewrite: CheckedPtr<void> void_ptr;
  CheckedPtr<void> void_ptr;

  // No rewrite expected (non-supported-type [1]).
  // TODO(lukasza): Skip function pointers.
  CheckedPtr<int (void)>*func_ptr)();
  int (MyStruct::*member_func_ptr)(char);

  // No rewrite expected (non-supported-type [1]).  Even with the indirection
  // via typedef or nesting inside another pointer type.
  // TODO(lukasza): Skip function pointers (in presence of typedefs).
  typedef void (*func_ptr_typedef)(char);
  func_ptr_typedef func_ptr_typedef_field1;
  CheckedPtr<MyStruct::func_ptr_typedef> func_ptr_typedef_field2;
};

// [1] non-supported-type - type that won't ever be either
// A) allocated by PartitionAlloc or B) derived from CheckedPtrSupport.
