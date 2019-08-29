// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CLANG_STACK_MAPS_OBJECTS_H_
#define TOOLS_CLANG_STACK_MAPS_OBJECTS_H_

#include <stdint.h>

#define GC_AS __attribute__((address_space(1)))
#define NO_STATEPOINT \
  __attribute__((noinline)) __attribute__((annotate("no-statepoint")))

using Address = uintptr_t;

// A HeapObject is just a heap allocated long integer. This is all that is
// necessary to show precise stack scanning in practise and greatly simplifies
// the implementation.
class HeapObject {
 public:
  HeapObject(long data) : data(data) {}
  long data;
};

template <typename T>
class Handle {
 public:
  static NO_STATEPOINT Handle<T> New(T* obj_ptr) {
    // We have to break the style guide here and do a C style cast because it
    // guarantees an address space cast takes place in the IR. reinterpret_cast
    // will fail to compile when address space qualifiers do not match.
    auto gcptr = (Address GC_AS*)obj_ptr;
    return Handle<T>(gcptr);
  }

  inline T* operator->() { return (T*)address; }

 private:
  Address GC_AS* address;
  Handle<T>(Address GC_AS* address) : address(address) {}
};

#endif  // TOOLS_CLANG_STACK_MAPS_OBJECTS_H_
