// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SomeClass;

class MyClass {
  MyClass() : ptr_field3_(nullptr), ptr_field6_(nullptr) {}

  // Expected rewrite: CheckedPtr<const SomeClass> ptr_field1_;
  const SomeClass* ptr_field1_;

  // Expected rewrite: CheckedPtr<volatile SomeClass> ptr_field2_;
  volatile SomeClass* ptr_field2_;

  // Expected rewrite: const CheckedPtr<SomeClass> ptr_field3_;
  //
  // TODO(lukasza): Fix this by using |qualType.getAsString|.
  // Currently the "outer" |const| is dropped.
  SomeClass* const ptr_field3_;

  // Expected rewrite: mutable CheckedPtr<SomeClass> ptr_field4_;
  //
  // TODO(lukasza): Fix this by looking at |field_decl->isMutable()|.
  // Currently the |mutable| specifier is dropped.
  mutable SomeClass* ptr_field4_;

  // Expected rewrite: CheckedPtr<const SomeClass> ptr_field5_;
  SomeClass const* ptr_field5_;

  // Expected rewrite: volatile CheckedPtr<const SomeClass> ptr_field6_;
  //
  // TODO(lukasza): Fix this by using |qualType.getAsString|.
  // Currently the "outer" qualifiers (like |volatile| below) are dropped.
  const SomeClass* volatile ptr_field6_;
};
