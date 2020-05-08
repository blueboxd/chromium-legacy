// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/profiler/unwinder.h"
#include "chrome/android/features/stack_unwinder/public/memory_regions_map.h"
#include "chrome/android/modules/stack_unwinder/internal/jni_headers/StackUnwinderModuleContentsImpl_jni.h"

std::unique_ptr<stack_unwinder::MemoryRegionsMap> CreateMemoryRegionsMap() {
  // TODO(etiennep): Implement.
  return nullptr;
}

std::unique_ptr<base::Unwinder> CreateNativeUnwinder(
    stack_unwinder::MemoryRegionsMap* memory_regions_map) {
  // TODO(etiennep): Implement.
  return nullptr;
}

static jlong
JNI_StackUnwinderModuleContentsImpl_GetCreateMemoryRegionsMapFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&CreateMemoryRegionsMap);
}

static jlong
JNI_StackUnwinderModuleContentsImpl_GetCreateNativeUnwinderFunction(
    JNIEnv* env) {
  return reinterpret_cast<jlong>(&CreateNativeUnwinder);
}
