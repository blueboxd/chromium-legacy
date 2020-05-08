// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_utils.h"
#include "base/profiler/unwinder.h"
#include "chrome/android/features/stack_unwinder/public/memory_regions_map.h"

extern "C" {
// This JNI registration method is found and called by module framework
// code. Empty because we have no JNI items to register within the module code.
JNI_GENERATOR_EXPORT bool JNI_OnLoad_stack_unwinder(JNIEnv* env) {
  return true;
}

// Native entry point functions.

// Creates a new memory regions map. The caller takes ownership of the returned
// object. We can't use std::unique_ptr here because this function has C
// linkage.
__attribute__((visibility("default"))) stack_unwinder::MemoryRegionsMap*
CreateMemoryRegionsMap() {
  // TODO(etiennep): Implement.
  return nullptr;
}

// Creates a new native unwinder. The caller takes ownership of the returned
// object. We can't use std::unique_ptr here because this function has C
// linkage.
__attribute__((visibility("default"))) base::Unwinder* CreateNativeUnwinder(
    stack_unwinder::MemoryRegionsMap* memory_regions_map) {
  // TODO(etiennep): Implement.
  return nullptr;
}

}  // extern "C"
