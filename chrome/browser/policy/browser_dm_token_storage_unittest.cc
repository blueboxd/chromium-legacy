// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/browser_dm_token_storage.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsEmpty;

namespace policy {

namespace {

constexpr char kClientId1[] = "fake-client-id-1";
constexpr char kClientId2[] = "fake-client-id-2";
constexpr char kEnrollmentToken1[] = "fake-enrollment-token-1";
constexpr char kEnrollmentToken2[] = "fake-enrollment-token-2";
constexpr char kDMToken1[] = "fake-dm-token-1";
constexpr char kDMToken2[] = "fake-dm-token-2";

}  // namespace

class BrowserDMTokenStorageTest : public testing::Test {
 public:
  BrowserDMTokenStorageTest()
      : storage_(kClientId1, kEnrollmentToken1, kDMToken1, false) {}
  FakeBrowserDMTokenStorage storage_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(BrowserDMTokenStorageTest, RetrieveClientId) {
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
  // The client ID value should be cached in memory and not read from the system
  // again.
  storage_.SetClientId(kClientId2);
  EXPECT_EQ(kClientId1, storage_.RetrieveClientId());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveEnrollmentToken) {
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());

  // The enrollment token should be cached in memory and not read from the
  // system again.
  storage_.SetEnrollmentToken(kEnrollmentToken2);
  EXPECT_EQ(kEnrollmentToken1, storage_.RetrieveEnrollmentToken());
}

TEST_F(BrowserDMTokenStorageTest, RetrieveDMToken) {
  EXPECT_EQ(kDMToken1, storage_.RetrieveDMToken());

  // The DM token should be cached in memory and not read from the system again.
  storage_.SetDMToken(kDMToken2);
  EXPECT_EQ(kDMToken1, storage_.RetrieveDMToken());
}

TEST_F(BrowserDMTokenStorageTest, ShouldDisplayErrorMessageOnFailure) {
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());

  // The error option should be cached in memory and not read from the system
  // again.
  storage_.SetEnrollmentErrorOption(true);
  EXPECT_FALSE(storage_.ShouldDisplayErrorMessageOnFailure());
}

}  // namespace policy
