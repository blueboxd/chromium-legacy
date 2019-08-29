// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/eol_notification.h"

#include "base/test/simple_test_clock.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/system_notification_helper.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_update_engine_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/devicetype_utils.h"

namespace chromeos {

class EolNotificationTest : public BrowserWithTestWindowTest {
 public:
  EolNotificationTest() = default;
  ~EolNotificationTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    TestingBrowserProcess::GetGlobal()->SetSystemNotificationHelper(
        std::make_unique<SystemNotificationHelper>());
    tester_ = std::make_unique<NotificationDisplayServiceTester>(profile());

    fake_update_engine_client_ = new FakeUpdateEngineClient();
    DBusThreadManager::GetSetterForTesting()->SetUpdateEngineClient(
        base::WrapUnique<UpdateEngineClient>(fake_update_engine_client_));

    eol_notification_ = std::make_unique<EolNotification>(profile());
    clock_ = std::make_unique<base::SimpleTestClock>();
    eol_notification_->clock_ = clock_.get();
  }

  void TearDown() override {
    eol_notification_.reset();
    tester_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  FakeUpdateEngineClient* fake_update_engine_client_;
  std::unique_ptr<NotificationDisplayServiceTester> tester_;
  std::unique_ptr<EolNotification> eol_notification_;
  std::unique_ptr<base::SimpleTestClock> clock_;
};

TEST_F(EolNotificationTest, TestMilestonesUntilEolNotification) {
  base::Time fake_time;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2019 12:00:00", &fake_time));
  clock_->SetNow(fake_time);

  fake_update_engine_client_->set_number_of_milestones(2);
  fake_update_engine_client_->set_end_of_life_status(
      update_engine::EndOfLifeStatus::kEol);
  eol_notification_->CheckEolStatus();

  // The callback passed from |eol_notification_| to
  // |fake_update_engine_client_| should be invoked before a notification can
  // appear.
  base::RunLoop().RunUntilIdle();

  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title = base::ASCIIToUTF16("Updates end March 2019");
  base::string16 expected_message = base::ASCIIToUTF16(
      "You'll still be able to use this Chrome device after that time, but it "
      "will no longer get automatic software and security updates");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);
}

TEST_F(EolNotificationTest, TestZeroMilestonesUntilEolNotification) {
  fake_update_engine_client_->set_number_of_milestones(0);
  fake_update_engine_client_->set_end_of_life_status(
      update_engine::EndOfLifeStatus::kEol);
  eol_notification_->CheckEolStatus();

  // The callback passed from |eol_notification_| to
  // |fake_update_engine_client_| should be invoked before a notification can
  // appear.
  base::RunLoop().RunUntilIdle();

  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title = base::ASCIIToUTF16("Final software update");
  base::string16 expected_message = base::ASCIIToUTF16(
      "This is the last automatic software and security update for this Chrome "
      "device. To get future updates, upgrade to a newer model.");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);
}

TEST_F(EolNotificationTest, TestEolNotificationWithoutMilestonesSet) {
  fake_update_engine_client_->set_end_of_life_status(
      update_engine::EndOfLifeStatus::kEol);
  eol_notification_->CheckEolStatus();

  // The callback passed from |eol_notification_| to
  // |fake_update_engine_client_| should be invoked before a notification can
  // appear.
  base::RunLoop().RunUntilIdle();

  auto notification = tester_->GetNotification("chrome://product_eol");
  ASSERT_TRUE(notification);

  base::string16 expected_title = base::ASCIIToUTF16("Final software update");
  base::string16 expected_message = base::ASCIIToUTF16(
      "This is the last automatic software and security update for this Chrome "
      "device. To get future updates, upgrade to a newer model.");
  EXPECT_EQ(notification->title(), expected_title);
  EXPECT_EQ(notification->message(), expected_message);
}

}  // namespace chromeos
