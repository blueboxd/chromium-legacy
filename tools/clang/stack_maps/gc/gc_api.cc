// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gc_api.h"
#include <ostream>

SafepointTable spt = GenSafepointTable();
Heap* heap = new Heap();

HeapAddress Heap::AllocRaw(long value) {
  assert(heap_ptr < kHeapSize && "Allocation failed: Heap full");

  HeapAddress raw_ptr = &fromspace()[heap_ptr];
  *raw_ptr = value;
  heap_ptr++;
  return raw_ptr;
}

void Heap::MoveObjects() {
  for (int i = 0; i < kHeapSize; i++) {
    auto tmp = a_frag_[i];
    a_frag_[i] = b_frag_[i];
    b_frag_[i] = tmp;
  }
  alloc_on_a_ = !alloc_on_a_;
}

HeapAddress Heap::UpdatePointer(HeapAddress ptr) {
  int offset =
      reinterpret_cast<char*>(ptr) - reinterpret_cast<char*>(fromspace());
  auto* new_ptr = reinterpret_cast<char*>(tospace()) + offset;
  return reinterpret_cast<HeapAddress>(new_ptr);
}

std::ostream& operator<<(std::ostream& os, const FrameRoots& fr) {
  os << "Register Roots: NYI" << std::endl << "\t";
  if (!fr.stack_roots_.size()) {
    os << "Stack Roots: []" << std::endl;
    return os;
  }

  os << "Stack Roots: [";
  for (auto SR : fr.stack_roots_) {
    os << "RBP - " << SR << ", ";
  }
  os << "\b\b]" << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, const SafepointTable& st) {
  os << "Safepoint Table:" << std::endl;
  for (auto const& pair : st.roots_) {
    os << "\t" << pair.first << ":\n\t" << pair.second << std::endl;
  }
  return os;
}

extern "C" void StackWalkAndMoveObjects(FramePtr fp) {
  while (true) {
    // The caller's return address is always 1 machine word above the recorded
    // RBP value in the current frame
    auto ra = reinterpret_cast<ReturnAddress>(*(fp + 1));

    printf("==== Frame %p ====\n", reinterpret_cast<void*>(ra));

    auto it = spt.roots()->find(ra);
    if (it != spt.roots()->end()) {
      auto fr_roots = it->second;
      for (auto root : *fr_roots.stack_roots()) {
        auto offset = root / sizeof(uintptr_t);
        auto stack_address = reinterpret_cast<uintptr_t*>((fp - offset));

        printf("\tRoot: [RBP - %d]\n", root);
        printf("\tAddress: %p\n", reinterpret_cast<void*>(*stack_address));

        // We know that all HeapObjects are wrappers around a single long
        // integer, so for debugging purposes we can cast it as such and print
        // the value to see if it looks correct.
        printf("\tValue: %ld\n",
               reinterpret_cast<HeapObject*>(*stack_address)->data);

        // We are in a collection, so we know that the underlying objects will
        // be moved before we return to the mutator. We update the on-stack
        // pointers here to point to the object's new location in the heap.
        HeapAddress new_ptr =
            heap->UpdatePointer(reinterpret_cast<HeapAddress>(*stack_address));
        *stack_address = reinterpret_cast<uintptr_t>(new_ptr);

        printf("\tAddress after Relocation: %p\n",
               reinterpret_cast<void*>(*stack_address));
      }
    }

    // Step up into the caller's frame or bail if we're at the top of stack
    fp = reinterpret_cast<FramePtr>(*fp);
    if (reinterpret_cast<uintptr_t>(fp) < spt.stack_begin())
      break;
  }

  heap->MoveObjects();
}

Handle<HeapObject> AllocateHeapObject(long data) {
  HeapAddress ptr = heap->AllocRaw(data);
  /* long* ptr = new long; */
  /* *ptr = data; */
  return Handle<HeapObject>::New(reinterpret_cast<HeapObject*>(ptr));
}
