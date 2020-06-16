// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file (and other gen-*-test.cc files) tests generation of output for
// --field-filter-file and therefore the expectations file
// (gen-in-out-arg-expected.txt) needs to be compared against the raw output of
// the rewriter (rather than against the actual edits result).  This makes the
// test incompatible with other tests, which require passing --apply-edits
// switch to test_tool.py and so to disable the test it is named *-test.cc
// rather than *-original.cc.
//
// To run the test use tools/clang/rewrite_raw_ptr_fields/tests/run_all_tests.py

namespace my_namespace {

class SomeClass;

struct MyStruct {
  SomeClass* ptr_field;
  SomeClass* in_out_via_ptr;
  SomeClass* in_out_via_ref;
  SomeClass* in_out_via_auto_reset;
};

template <typename T>
class AutoReset {
 public:
  AutoReset(T* ptr, T value) : ptr_(ptr), value_(value) {}
  ~AutoReset() { *ptr_ = value_; }

 private:
  T* ptr_;
  T value_;
};

void GetViaPtr(SomeClass** out_ptr) {
  *out_ptr = nullptr;
}

// Based on a real-world example (Blink uses references more often than the rest
// of Chromium):
// https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/core/layout/layout_table.cc;drc=a3524fd6d1a4f4ff7e97893f6c6375dd1684e132;l=130
void GetViaRef(SomeClass*& out_ptr) {
  out_ptr = nullptr;
}

void foo() {
  MyStruct my_struct;
  GetViaPtr(&my_struct.in_out_via_ptr);
  GetViaRef(my_struct.in_out_via_ref);
  AutoReset<SomeClass*> auto_reset1(&my_struct.in_out_via_auto_reset, nullptr);
}

template <typename T>
class MyTemplateBase {
 protected:
  T* ptr_;
};

class MyTemplateDerived : public MyTemplateBase<SomeClass> {
 public:
  void foo() {
    // This should emit
    //     my_namespace MyTemplateBase<T>::ptr_
    // rather than
    //     my_namespace MyTemplateBase<SomeClass>::ptr_
    GetViaPtr(&ptr_);
  }
};

}  // namespace my_namespace
