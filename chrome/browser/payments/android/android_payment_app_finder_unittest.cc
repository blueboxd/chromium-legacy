// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/native_j_unittests_jni_headers/AndroidPaymentAppFinderUnitTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class AndroidPaymentAppFinderUnitTest : public ::testing::Test {
 public:
  AndroidPaymentAppFinderUnitTest()
      : j_test_(Java_AndroidPaymentAppFinderUnitTest_Constructor(
            AttachCurrentThread())) {}

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

JAVA_TESTS(AndroidPaymentAppFinderUnitTest, j_test())
