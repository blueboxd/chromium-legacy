// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_RANDOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_RANDOM_H_

#include <stdint.h>

#include "base/base_export.h"

namespace base {

struct RandomContext;

// TODO(crbug.com/984742): This doesn't need to be part of the interface, and
// can be an implementation detail of `RandomValue`.
//
// Returns a pointer to the global `RandomContext`. If the context has not been
// initialized yet, initializes it with a random starting state.
RandomContext* GetRandomContext();

uint32_t RandomValue(RandomContext* x);

// TODO(crbug.com/984742): Rename this to `SetRandomSeedForTesting`.
//
// Sets the seed for the random number generator to a known value, to cause the
// RNG to generate a predictable sequence of outputs. May be called multiple
// times.
BASE_EXPORT void SetRandomPageBaseSeed(int64_t seed);

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_RANDOM_H_
