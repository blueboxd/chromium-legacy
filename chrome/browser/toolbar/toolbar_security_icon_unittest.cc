// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/native_j_unittests_jni_headers/ToolbarSecurityIconTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class ToolbarSecurityIconTest : public ::testing::Test {
 public:
  ToolbarSecurityIconTest()
      : j_test_(
            Java_ToolbarSecurityIconTest_Constructor(AttachCurrentThread())) {}

  void SetUp() override {
    Java_ToolbarSecurityIconTest_setUp(AttachCurrentThread(), j_test_);
  }

  void TearDown() override {
    Java_ToolbarSecurityIconTest_tearDown(AttachCurrentThread(), j_test_);
  }

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

JAVA_TESTS(ToolbarSecurityIconTest, j_test())
