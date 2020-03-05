// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GC_TEST_BASE_ALLOCATOR_H_
#define COMPONENTS_GC_TEST_BASE_ALLOCATOR_H_

#include "components/gc/public/platform.h"

namespace gc {
namespace test {

class BaseAllocator final : public PageAllocator {
 public:
  size_t AllocatePageSize() const final;
  size_t CommitPageSize() const final;

  void* AllocatePages(void*, size_t, size_t, Permission) final;
  bool FreePages(void*, size_t) final;
  bool SetPermissions(void*, size_t, Permission) final;
  bool DiscardSystemPages(void*, size_t) final;
};

}  // namespace test
}  // namespace gc

#endif  // COMPONENTS_GC_TEST_BASE_ALLOCATOR_H_
