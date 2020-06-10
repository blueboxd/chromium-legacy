// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/bulk_leak_check_controller_android.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::BulkLeakCheckService;
using password_manager::IsLeaked;
using password_manager::LeakCheckCredential;
using ::testing::AtLeast;
using ::testing::NiceMock;

namespace {
class MockObserver : public BulkLeakCheckControllerAndroid::Observer {
 public:
  MOCK_METHOD(void,
              OnStateChanged,
              (BulkLeakCheckService::State state),
              (override));

  MOCK_METHOD(void,
              OnCredentialDone,
              (const LeakCheckCredential& credential,
               IsLeaked is_leaked,
               DoneCount credentials_checked,
               TotalCount total_to_check),
              (override));
};
}  // namespace

class BulkLeakCheckControllerAndroidTest : public testing::Test {
 public:
  BulkLeakCheckControllerAndroidTest() { controller_.AddObserver(&observer_); }

 protected:
  BulkLeakCheckControllerAndroid controller_;
  NiceMock<MockObserver> observer_;
};

TEST_F(BulkLeakCheckControllerAndroidTest, StartPasswordCheck) {
  EXPECT_CALL(observer_, OnStateChanged(BulkLeakCheckService::State::kIdle))
      .Times(1);
  controller_.StartPasswordCheck();
}

TEST_F(BulkLeakCheckControllerAndroidTest, GetNumberOfSavedPasswords) {
  EXPECT_EQ(0, controller_.GetNumberOfSavedPasswords());
}

TEST_F(BulkLeakCheckControllerAndroidTest, GetNumberOfLeaksFromLastCheck) {
  EXPECT_EQ(0, controller_.GetNumberOfLeaksFromLastCheck());
}
