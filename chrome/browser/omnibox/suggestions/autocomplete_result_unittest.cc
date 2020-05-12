// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/android/native_j_unittests_jni_headers/AutocompleteResultUnitTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class AutocompleteResultUnitTest : public ::testing::Test {
 public:
  AutocompleteResultUnitTest()
      : j_test_(Java_AutocompleteResultUnitTest_Constructor(
            AttachCurrentThread())) {}

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

JAVA_TESTS(AutocompleteResultUnitTest, j_test())
