// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gc_api.h"
#include <ostream>

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

extern "C" void StackWalk(FramePtr fp) {
  while (true) {
    // The caller's return address is always 1 machine word above the recorded
    // RBP value in the current frame
    auto ra = reinterpret_cast<ReturnAddress>(*(fp + 1));

    printf("==== Frame %p ====", reinterpret_cast<void*>(ra));

    auto it = spt.roots()->find(ra);
    if (it != spt.roots()->end()) {
      auto fr_roots = it->second;
      for (auto root : *fr_roots.stack_roots()) {
        auto offset = root / sizeof(uintptr_t);
        auto address = reinterpret_cast<uintptr_t>(fp - offset);

        printf("\tRoot: [RBP - %d]", root);
        printf("\tAddress: %p", reinterpret_cast<void*>(address));
      }
    }

    // Step up into the caller's frame or bail if we're at the top of stack
    fp = reinterpret_cast<FramePtr>(*fp);
    if (reinterpret_cast<uintptr_t>(fp) < spt.stack_begin())
      break;
  }
}
