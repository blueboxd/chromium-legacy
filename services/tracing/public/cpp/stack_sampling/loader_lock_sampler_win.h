// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_

#include "base/component_export.h"

namespace tracing {

// Ensures the mechanism that samples the loader lock is initialized. This can
// be called multiple times but only the first call in a process has any
// effect. This may take the loader lock itself so it must be called before
// sampling starts.
COMPONENT_EXPORT(TRACING_CPP) void InitializeLoaderLockSampling();

// Checks if the loader lock is currently held by another thread. Returns false
// if the lock is already held by the calling thread since the lock can be
// taken recursively. InitializeLoaderLockSampling must be called before this.
COMPONENT_EXPORT(TRACING_CPP) bool IsLoaderLockHeld();

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_LOADER_LOCK_SAMPLER_WIN_H_
