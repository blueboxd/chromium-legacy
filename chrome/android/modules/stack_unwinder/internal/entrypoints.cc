// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_generator/jni_generator_helper.h"
#include "base/android/jni_utils.h"
#include "base/profiler/unwinder.h"

extern "C" {
// This JNI registration method is found and called by module framework
// code. Empty because we have no JNI items to register within the module code.
JNI_GENERATOR_EXPORT bool JNI_OnLoad_stack_unwinder(JNIEnv* env) {
  return true;
}

}  // extern "C"
