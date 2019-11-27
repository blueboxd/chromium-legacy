// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/android/features/stack_unwinder/internal/jni_headers/StackUnwinderImpl_jni.h"

static void JNI_StackUnwinderImpl_RegisterUnwinder(JNIEnv* env) {
  // TODO(wittman): Create the native Android unwinder and register with
  // base::StackSamplingProfiler.
}
