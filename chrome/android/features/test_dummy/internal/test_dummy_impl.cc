// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/android/features/test_dummy/internal/jni_headers/TestDummyImpl_jni.h"

static int JNI_TestDummyImpl_Execute(JNIEnv* env) {
  LOG(INFO) << "Running test dummy native library";
  return 123;
}
