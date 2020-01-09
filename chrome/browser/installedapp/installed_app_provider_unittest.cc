// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/android/native_j_unittests_jni_headers/InstalledAppProviderTest_jni.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::android::AttachCurrentThread;

class InstalledAppProviderTest : public ::testing::Test {
 public:
  InstalledAppProviderTest() {
    JNIEnv* env = AttachCurrentThread();
    j_test_.Reset(Java_InstalledAppProviderTest_create(env));
  }

  void SetUp() override {
    JNIEnv* env = AttachCurrentThread();
    Java_InstalledAppProviderTest_setUp(env, j_test_);
  }

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

TEST_F(InstalledAppProviderTest, TestOriginMissingParts) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOriginMissingParts(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestIncognitoWithOneInstalledRelatedApp) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testIncognitoWithOneInstalledRelatedApp(
      env, j_test());
}

TEST_F(InstalledAppProviderTest, TestNoRelatedApps) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testNoRelatedApps(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneRelatedAppNoId) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppNoId(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneRelatedNonAndroidApp) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedNonAndroidApp(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneRelatedAppNotInstalled) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppNotInstalled(env, j_test());
}

TEST_F(InstalledAppProviderTest,
       TestOneRelatedAppBrokenAssetStatementsResource) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppBrokenAssetStatementsResource(
      env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneRelatedAppNoAssetStatements) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppNoAssetStatements(env,
                                                                   j_test());
}

TEST_F(InstalledAppProviderTest,
       TestOneRelatedAppNoAssetStatementsNullMetadata) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppNoAssetStatementsNullMetadata(
      env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneRelatedAppRelatedToDifferentOrigins) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneRelatedAppRelatedToDifferentOrigins(
      env, j_test());
}

TEST_F(InstalledAppProviderTest, TestOneInstalledRelatedApp) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testOneInstalledRelatedApp(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestDynamicallyChangingUrl) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testDynamicallyChangingUrl(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestInstalledRelatedAppWithUrl) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testInstalledRelatedAppWithUrl(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestMultipleAssetStatements) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testMultipleAssetStatements(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementSyntaxError) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementSyntaxError(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNotArray) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNotArray(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementArrayNoObjects) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementArrayNoObjects(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNoRelation) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNoRelation(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNonStandardRelation) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNonStandardRelation(env,
                                                                      j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNoTarget) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNoTarget(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNoNamespace) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNoNamespace(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestNonWebAssetStatement) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testNonWebAssetStatement(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementNoSite) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementNoSite(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementSiteSyntaxError) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementSiteSyntaxError(env,
                                                                  j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementSiteMissingParts) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementSiteMissingParts(env,
                                                                   j_test());
}

TEST_F(InstalledAppProviderTest, TestAssetStatementSiteHasPath) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testAssetStatementSiteHasPath(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestExtraInstalledApp) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testExtraInstalledApp(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestMultipleInstalledRelatedApps) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testMultipleInstalledRelatedApps(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestArtificialDelay) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testArtificialDelay(env, j_test());
}

TEST_F(InstalledAppProviderTest, TestMultipleAppsIncludingInstantApps) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testMultipleAppsIncludingInstantApps(env,
                                                                     j_test());
}

TEST_F(InstalledAppProviderTest, TestRelatedAppsOverAllowedThreshold) {
  JNIEnv* env = AttachCurrentThread();
  Java_InstalledAppProviderTest_testRelatedAppsOverAllowedThreshold(env,
                                                                    j_test());
}
