// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/dlext.h>
#include <dlfcn.h>

#include "base/android/bundle_utils.h"
#include "base/logging.h"
#include "chrome/android/features/test_dummy/internal/buildflags.h"
#include "chrome/android/features/test_dummy/internal/test_dummy_jni_headers/TestDummySupport_jni.h"

static jboolean JNI_TestDummySupport_OpenAndVerifyNativeLibrary(JNIEnv* env) {
#if BUILDFLAG(USE_NATIVE_MODULES)
  // Note that the library opened here is not closed. dlclosing() libraries has
  // proven to be problematic. See https://crbug.com/994029.
  void* handle =
      base::android::BundleUtils::DlOpenModuleLibraryPartition("test_dummy");
  if (handle == nullptr) {
    LOG(ERROR) << "Cannot open test library: " << dlerror();
    return false;
  }

  void* symbol = dlsym(handle, "TestDummyEntrypoint");
  if (symbol == nullptr) {
    LOG(ERROR) << "Cannot find test library symbol";
    return false;
  }

  typedef int TestFunction();
  TestFunction* test_function = reinterpret_cast<TestFunction*>(symbol);
  if (test_function() != 123) {
    LOG(ERROR) << "Unexpected value from test library";
    return false;
  }
#endif

  return true;
}
