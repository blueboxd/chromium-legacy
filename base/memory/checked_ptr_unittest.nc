// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include <tuple>  // for std::ignore

#include "base/memory/checked_ptr.h"

namespace base {

struct Producer {};
struct DerivedProducer : Producer {};
struct OtherDerivedProducer : Producer {};
struct Unrelated {};
struct DerivedUnrelated : Unrelated {};

#if defined(NCTEST_AUTO_DOWNCAST)  // [r"no viable conversion from 'CheckedPtr<base::Producer>' to 'CheckedPtr<base::DerivedProducer>'"]

void WontCompile() {
  Producer f;
  CheckedPtr<Producer> ptr = &f;
  CheckedPtr<DerivedProducer> derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_DOWNCAST)  // [r"no matching conversion for static_cast from 'CheckedPtr<base::Producer>' to 'CheckedPtr<base::DerivedProducer>'"]

void WontCompile() {
  Producer f;
  CheckedPtr<Producer> ptr = &f;
  CheckedPtr<DerivedProducer> derived_ptr =
      static_cast<CheckedPtr<DerivedProducer>>(ptr);
}

#elif defined(NCTEST_AUTO_REF_DOWNCAST)  // [r"non-const lvalue reference to type 'CheckedPtr<base::DerivedProducer>' cannot bind to a value of unrelated type 'CheckedPtr<base::Producer>'"]

void WontCompile() {
  Producer f;
  CheckedPtr<Producer> ptr = &f;
  CheckedPtr<DerivedProducer>& derived_ptr = ptr;
}

#elif defined(NCTEST_STATIC_REF_DOWNCAST)  // [r"non-const lvalue reference to type 'CheckedPtr<base::DerivedProducer>' cannot bind to a value of unrelated type 'CheckedPtr<base::Producer>'"]

void WontCompile() {
  Producer f;
  CheckedPtr<Producer> ptr = &f;
  CheckedPtr<DerivedProducer>& derived_ptr =
      static_cast<CheckedPtr<DerivedProducer>&>(ptr);
}

#elif defined(NCTEST_AUTO_DOWNCAST_FROM_RAW) // [r"no viable conversion from 'base::Producer \*' to 'CheckedPtr<base::DerivedProducer>'"]

void WontCompile() {
  Producer f;
  CheckedPtr<DerivedProducer> ptr = &f;
}

#elif defined(NCTEST_UNRELATED_FROM_RAW) // [r"no viable conversion from 'base::DerivedProducer \*' to 'CheckedPtr<base::Unrelated>'"]

void WontCompile() {
  DerivedProducer f;
  CheckedPtr<Unrelated> ptr = &f;
}

#elif defined(NCTEST_UNRELATED_STATIC_FROM_WRAPPED) // [r"static_cast from 'base::DerivedProducer \*' to 'base::Unrelated \*', which are not related by inheritance, is not allowed"]

void WontCompile() {
  DerivedProducer f;
  CheckedPtr<DerivedProducer> ptr = &f;
  std::ignore = static_cast<Unrelated*>(ptr);
}

#elif defined(NCTEST_VOID_DEREFERENCE) // [r"indirection requires pointer operand \('CheckedPtr<const void>' invalid\)"]

void WontCompile() {
  const char foo[] = "42";
  CheckedPtr<const void> ptr = foo;
  std::ignore = *ptr;
}

#endif

}  // namespace base
