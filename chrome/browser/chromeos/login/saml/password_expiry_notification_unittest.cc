// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/saml/password_expiry_notification.h"

#include "ash/public/cpp/session/session_activation_observer.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/login/auth/saml_password_attributes.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using message_center::Notification;

namespace chromeos {

namespace {

inline base::string16 utf16(const char* ascii) {
  return base::ASCIIToUTF16(ascii);
}

class SamlPasswordExpiryNotificationTest : public testing::Test {
 protected:
  base::Optional<Notification> Notification() {
    return NotificationDisplayServiceTester::Get()->GetNotification(
        "saml.password-expiry-notification");
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  NotificationDisplayServiceTester display_service_tester_{&profile_};
};

}  // namespace

TEST_F(SamlPasswordExpiryNotificationTest, ShowWillSoonExpire) {
  PasswordExpiryNotification::Show(&profile_, 14);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password expires in less than 14 days"),
            Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

TEST_F(SamlPasswordExpiryNotificationTest, ShowAlreadyExpired) {
  PasswordExpiryNotification::Show(&profile_, 0);
  ASSERT_TRUE(Notification().has_value());

  EXPECT_EQ(utf16("Password is expired"), Notification()->title());
  EXPECT_EQ(utf16("Choose a new one now"), Notification()->message());

  PasswordExpiryNotification::Dismiss(&profile_);
  EXPECT_FALSE(Notification().has_value());
}

}  // namespace chromeos
