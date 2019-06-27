// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_notification_controller.h"

#include <memory>
#include <string>

#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

struct WilcoDtcSupportdNotificationControllerTestParams {
  std::string (WilcoDtcSupportdNotificationController::*function)() const;
  const int title;
  const int message;
  const message_center::NotificationPriority priority;
} kTestParams[] = {
    {&WilcoDtcSupportdNotificationController::ShowBatteryAuthNotification,
     IDS_WILCO_NOTIFICATION_BATTERY_AUTH_TITLE,
     IDS_WILCO_NOTIFICATION_BATTERY_AUTH_MESSAGE,
     message_center::SYSTEM_PRIORITY},
    {&WilcoDtcSupportdNotificationController::ShowNonWilcoChargerNotification,
     IDS_WILCO_NOTIFICATION_NON_WILCO_CHARGER_TITLE,
     IDS_WILCO_NOTIFICATION_NON_WILCO_CHARGER_MESSAGE,
     message_center::DEFAULT_PRIORITY},
    {&WilcoDtcSupportdNotificationController::ShowIncompatibleDockNotification,
     IDS_WILCO_NOTIFICATION_INCOMPATIBLE_DOCK_TITLE,
     IDS_WILCO_NOTIFICATION_INCOMPATIBLE_DOCK_MESSAGE,
     message_center::DEFAULT_PRIORITY},
    {&WilcoDtcSupportdNotificationController::ShowDockErrorNotification,
     IDS_WILCO_NOTIFICATION_DOCK_ERROR_TITLE,
     IDS_WILCO_NOTIFICATION_DOCK_ERROR_MESSAGE,
     message_center::DEFAULT_PRIORITY},
    {&WilcoDtcSupportdNotificationController::ShowDockDisplayNotification,
     IDS_WILCO_NOTIFICATION_DOCK_DISPLAY_TITLE,
     IDS_WILCO_NOTIFICATION_DOCK_DISPLAY_MESSAGE,
     message_center::DEFAULT_PRIORITY},
    {&WilcoDtcSupportdNotificationController::ShowDockThunderboltNotification,
     IDS_WILCO_NOTIFICATION_DOCK_THUNDERBOLT_TITLE,
     IDS_WILCO_NOTIFICATION_DOCK_THUNDERBOLT_MESSAGE,
     message_center::DEFAULT_PRIORITY}};

class WilcoDtcSupportdNotificationControllerTest
    : public testing::TestWithParam<
          struct WilcoDtcSupportdNotificationControllerTestParams> {
 public:
  WilcoDtcSupportdNotificationControllerTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    CHECK(profile_manager_.SetUp());

    const char kProfileName[] = "test@example.com";
    // The created profile is owned by ProfileManager.
    Profile* profile = profile_manager_.CreateTestingProfile(kProfileName);
    profile_manager_.UpdateLastUser(profile);
    profile_manager_.SetLoggedIn(true);
    ProfileHelper::Get()->SetActiveUserIdForTesting(kProfileName);
    service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(profile);

    notification_controller_ =
        std::make_unique<WilcoDtcSupportdNotificationController>(
            profile_manager_.profile_manager());
  }

 protected:
  WilcoDtcSupportdNotificationController* notification_controller() const {
    return notification_controller_.get();
  }

  size_t NotificationCount() const {
    return service_tester_
        ->GetDisplayedNotificationsForType(NotificationHandler::Type::TRANSIENT)
        .size();
  }

  base::Optional<message_center::Notification> GetNotification(
      const std::string& notification_id) const {
    return service_tester_->GetNotification(notification_id);
  }

  void RemoveNotification(const std::string& notification_id) {
    service_tester_->RemoveNotification(NotificationHandler::Type::TRANSIENT,
                                        notification_id, false);
  }

 private:
  // CreateTestingProfile must be called on Chrome_UIThread
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<WilcoDtcSupportdNotificationController>
      notification_controller_ = nullptr;
  std::unique_ptr<NotificationDisplayServiceTester> service_tester_ = nullptr;
};

}  // namespace

TEST_P(WilcoDtcSupportdNotificationControllerTest, SingleNotification) {
  WilcoDtcSupportdNotificationControllerTestParams test_params = GetParam();
  EXPECT_EQ(0u, NotificationCount());
  std::string id = (notification_controller()->*test_params.function)();
  EXPECT_EQ(1u, NotificationCount());
  base::Optional<message_center::Notification> notification =
      GetNotification(id);
  EXPECT_EQ(l10n_util::GetStringUTF16(test_params.title),
            notification->title());
  EXPECT_EQ(l10n_util::GetStringUTF16(test_params.message),
            notification->message());
  EXPECT_EQ(test_params.priority, notification->priority());
  RemoveNotification(id);
  EXPECT_EQ(0u, NotificationCount());
}

TEST_P(WilcoDtcSupportdNotificationControllerTest, DoubleNotification) {
  WilcoDtcSupportdNotificationControllerTestParams test_params = GetParam();
  EXPECT_EQ(0u, NotificationCount());
  std::string id = (notification_controller()->*test_params.function)();
  (notification_controller()->*test_params.function)();
  EXPECT_EQ(1u, NotificationCount());
  RemoveNotification(id);
  EXPECT_EQ(0u, NotificationCount());
}

INSTANTIATE_TEST_SUITE_P(WilcoDtcSupportdNotificationControllerTest,
                         WilcoDtcSupportdNotificationControllerTest,
                         testing::ValuesIn(kTestParams));

}  // namespace chromeos
