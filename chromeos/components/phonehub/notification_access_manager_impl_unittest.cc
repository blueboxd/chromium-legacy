// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/phonehub/notification_access_manager_impl.h"

#include <memory>

#include "chromeos/components/phonehub/notification_access_setup_operation.h"
#include "chromeos/components/phonehub/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace phonehub {
namespace {

class FakeObserver : public NotificationAccessManager::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_calls() const { return num_calls_; }

  // NotificationAccessManager::Observer:
  void OnNotificationAccessChanged() override { ++num_calls_; }

 private:
  size_t num_calls_ = 0;
};

class FakeOperationDelegate
    : public NotificationAccessSetupOperation::Delegate {
 public:
  FakeOperationDelegate() = default;
  ~FakeOperationDelegate() override = default;

  NotificationAccessSetupOperation::Status status() const { return status_; }

  // NotificationAccessSetupOperation::Delegate:
  void OnStatusChange(
      NotificationAccessSetupOperation::Status new_status) override {
    status_ = new_status;
  }

 private:
  NotificationAccessSetupOperation::Status status_ =
      NotificationAccessSetupOperation::Status::kConnecting;
};

}  // namespace

class NotificationAccessManagerImplTest : public testing::Test {
 protected:
  NotificationAccessManagerImplTest() = default;
  NotificationAccessManagerImplTest(const NotificationAccessManagerImplTest&) =
      delete;
  NotificationAccessManagerImplTest& operator=(
      const NotificationAccessManagerImplTest&) = delete;
  ~NotificationAccessManagerImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    NotificationAccessManagerImpl::RegisterPrefs(pref_service_.registry());
  }

  void Initialize(bool initial_has_access_been_granted) {
    pref_service_.SetBoolean(prefs::kNotificationAccessGranted,
                             initial_has_access_been_granted);
    manager_ = std::make_unique<NotificationAccessManagerImpl>(&pref_service_);
  }

  std::unique_ptr<NotificationAccessSetupOperation> StartSetupOperation() {
    return manager_->AttemptNotificationSetup(&fake_delegate_);
  }

  bool GetHasAccessBeenGranted() { return manager_->HasAccessBeenGranted(); }
  bool IsSetupOperationInProgress() {
    return manager_->IsSetupOperationInProgress();
  }

  size_t GetNumObserverCalls() const { return fake_observer_.num_calls(); }

 private:
  TestingPrefServiceSimple pref_service_;

  FakeObserver fake_observer_;
  FakeOperationDelegate fake_delegate_;
  std::unique_ptr<NotificationAccessManager> manager_;
};

TEST_F(NotificationAccessManagerImplTest, InitiallyGranted) {
  Initialize(/*initial_has_access_been_granted=*/true);
  EXPECT_TRUE(GetHasAccessBeenGranted());

  // Cannot start the notification access setup flow if access has already been
  // granted.
  auto operation = StartSetupOperation();
  EXPECT_FALSE(operation);
}

TEST_F(NotificationAccessManagerImplTest, InitiallyNotGranted) {
  Initialize(/*initial_has_access_been_granted=*/false);
  EXPECT_FALSE(GetHasAccessBeenGranted());

  auto operation = StartSetupOperation();
  EXPECT_TRUE(operation);
  EXPECT_TRUE(IsSetupOperationInProgress());

  operation.reset();
  EXPECT_FALSE(IsSetupOperationInProgress());
}

}  // namespace phonehub
}  // namespace chromeos
