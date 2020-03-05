// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GC_CORE_GLOBALS_H_
#define COMPONENTS_GC_CORE_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

namespace gc {
namespace internal {

using Address = uint8_t*;

// Page size of normal pages used for allocation. Actually usable area on the
// page depends on pager headers and guard pages.
constexpr size_t kPageSizeLog2 = 17;
constexpr size_t kPageSize = 1 << kPageSizeLog2;  // 128 KiB.
constexpr size_t kPageOffsetMask = kPageSize - 1;
constexpr size_t kPageBaseMask = ~kPageOffsetMask;

// Guard pages are always put into memory. Whether they are actually protected
// depends on the allocator provided to the garbage collector.
constexpr size_t kGuardPageSize = 4096;

static_assert((kPageSize & (kPageSize - 1)) == 0,
              "kPageSize must be power of 2");
static_assert((kGuardPageSize & (kGuardPageSize - 1)) == 0,
              "kGuardPageSize must be power of 2");

}  // namespace internal
}  // namespace gc

#endif  // COMPONENTS_GC_CORE_GLOBALS_H_
