// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {

struct Feature;

extern const BASE_EXPORT Feature kPartitionAllocGigaCage;

ALWAYS_INLINE bool IsPartitionAllocGigaCageEnabled() {
  // The feature is not applicable to 32 bit architectures (not enough address
  // space).
  //
  // It also cannot be enabled conditionally when PartitionAlloc is the default
  // allocator, as base::Feature allocates. However as this is the intended use
  // case, we enable it for all builds then.
#if !(defined(ARCH_CPU_64_BITS) && !defined(OS_NACL))
  return false;
#elif BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return true;
#else
  return FeatureList::IsEnabled(kPartitionAllocGigaCage);
#endif
}

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
