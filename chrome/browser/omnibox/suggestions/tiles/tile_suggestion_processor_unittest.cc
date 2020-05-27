// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/android/native_j_unittests_jni_headers/TileSuggestionProcessorUnitTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class TileSuggestionProcessorUnitTest : public ::testing::Test {
 public:
  TileSuggestionProcessorUnitTest()
      : j_test_(Java_TileSuggestionProcessorUnitTest_Constructor(
            AttachCurrentThread())) {}

  void SetUp() override {
    Java_TileSuggestionProcessorUnitTest_setUp(AttachCurrentThread(), j_test_);
  }

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

JAVA_TESTS(TileSuggestionProcessorUnitTest, j_test())
