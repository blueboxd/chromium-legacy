// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GC_PUBLIC_PLATFORM_H_
#define COMPONENTS_GC_PUBLIC_PLATFORM_H_

#include <stddef.h>

#include "components/gc/core/gc_export.h"

namespace gc {

// Allocator used to get memory from the embedder.
class GC_EXPORT PageAllocator {
 public:
  // Memory permissions.
  enum Permission { kNoAccess, kRead, kReadWrite, kReadExecute };

  virtual ~PageAllocator() = default;

  // Page granularity for |AllocatePages()| and |FreePages()|. Addresses and
  // lengths must be multiples of |AllocatePageSize()|.
  virtual size_t AllocatePageSize() const = 0;

  // Page granularity for |SetPermissions()| and |DiscardSystemPages()|.
  // Addresses and lengths must be multiples of |CommitPageSize()|.
  virtual size_t CommitPageSize() const = 0;

  // Allocates memory at the given |address| (hint) with the provided |length|,
  // |alignment|, and |permissions|.
  virtual void* AllocatePages(void* address,
                              size_t length,
                              size_t alignment,
                              Permission permissions) = 0;

  // Frees memory in a range that was allocated by |AllocatedPages()|.
  virtual bool FreePages(void* address, size_t length) = 0;

  // Sets permissions in a range that was allocated by |AllocatedPages()|.
  virtual bool SetPermissions(void* address,
                              size_t length,
                              Permission permissions) = 0;

  // Potentially frees physical memory in the range [address, address+length).
  // Address and size should be aligned with |CommitPageSize()|. Note that this
  // call transparently brings back physical memory at an unknown state.
  //
  // Returns true on success, and false otherwise.
  virtual bool DiscardSystemPages(void* address, size_t size) = 0;
};

}  // namespace gc

#endif  // COMPONENTS_GC_PUBLIC_PLATFORM_H_
