// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/native_j_unittests_jni_headers/AndroidPaymentAppUnitTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

class AndroidPaymentAppUnitTest : public ::testing::Test {
 public:
  AndroidPaymentAppUnitTest()
      : j_test_(Java_AndroidPaymentAppUnitTest_Constructor(
            base::android::AttachCurrentThread())) {}

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

JAVA_TESTS(AndroidPaymentAppUnitTest, j_test())
