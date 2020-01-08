// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/native_j_unittests_jni_headers/ChromeContextMenuPopulatorTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class ChromeContextMenuPopulatorTest : public ::testing::Test {
 public:
  ChromeContextMenuPopulatorTest() {
    JNIEnv* env = AttachCurrentThread();
    j_test_.Reset(Java_ChromeContextMenuPopulatorTest_create(env));
  }

  void SetUp() override {
    JNIEnv* env = AttachCurrentThread();
    Java_ChromeContextMenuPopulatorTest_setUp(env, j_test_);
  }

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

TEST_F(ChromeContextMenuPopulatorTest, TestHttpLink) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testHttpLink(env, j_test());
}

TEST_F(ChromeContextMenuPopulatorTest, TestMailLink) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testMailLink(env, j_test());
}

TEST_F(ChromeContextMenuPopulatorTest, TestTelLink) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testTelLink(env, j_test());
}

TEST_F(ChromeContextMenuPopulatorTest, TestVideoLink) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testVideoLink(env, j_test());
}

TEST_F(ChromeContextMenuPopulatorTest, TestImageHiFi) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testImageHiFi(env, j_test());
}

TEST_F(ChromeContextMenuPopulatorTest, TestHttpLinkWithImageHiFi) {
  JNIEnv* env = AttachCurrentThread();
  Java_ChromeContextMenuPopulatorTest_testHttpLinkWithImageHiFi(env, j_test());
}
