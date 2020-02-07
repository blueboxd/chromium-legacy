// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_SIGNIN_UTILS_ANDROID_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_SIGNIN_UTILS_ANDROID_H_

// Utilities that are an interface with java to sign-in a test account for Sync
// testing on Android.

namespace sync_test_signin_utils_android {

// Sets up the test account and signs in synchronously.
void SetUpTestAccountAndSignIn();

// Sets up the test authentication environment synchronously using a worker
// thread.
//
// We recommend to call this function from the SetUp() method of the test
// fixture (e.g., CustomFixture::SetUp()) before calling the other SetUp()
// function down the stack (e.g., PlatformBrowserTest::SetUp()).
void SetUpAuthForTest();

// Tears down the test authentication environment synchronously using a worker
// thread.
//
// We recommend to call this function from the PostRunTestOnMainThread() method
// of the test fixture which allows multiple threads, see example
// chrome/browser/metrics/metrics_service_user_demographics_browsertest.cc.
void TearDownAuthForTest();

}  // namespace sync_test_signin_utils_android

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_TEST_SIGNIN_UTILS_ANDROID_H_
