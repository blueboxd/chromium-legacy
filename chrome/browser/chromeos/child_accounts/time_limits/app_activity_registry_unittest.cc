// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"

#include <map>
#include <memory>
#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace {

const chromeos::app_time::AppId kApp1(apps::mojom::AppType::kArc, "1");
const chromeos::app_time::AppId kApp2(apps::mojom::AppType::kWeb, "3");

class AppTimeNotificationDelegateMock
    : public chromeos::app_time::AppTimeNotificationDelegate {
 public:
  AppTimeNotificationDelegateMock() = default;
  AppTimeNotificationDelegateMock(const AppTimeNotificationDelegateMock&) =
      delete;
  AppTimeNotificationDelegateMock& operator=(
      const AppTimeNotificationDelegateMock&) = delete;

  ~AppTimeNotificationDelegateMock() = default;

  MOCK_METHOD2(ShowAppTimeLimitNotification,
               void(const chromeos::app_time::AppId&,
                    chromeos::app_time::AppNotification));
};

}  // namespace

class AppActivityRegistryTest : public ChromeViewsTestBase {
 protected:
  AppActivityRegistryTest()
      : ChromeViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  AppActivityRegistryTest(const AppActivityRegistryTest&) = delete;
  AppActivityRegistryTest& operator=(const AppActivityRegistryTest&) = delete;
  ~AppActivityRegistryTest() override = default;

  // ChromeViewsTestBase:
  void SetUp() override;

  aura::Window* CreateWindowForApp(const chromeos::app_time::AppId& app_id);

  chromeos::app_time::AppActivityRegistry& app_activity_registry() {
    return registry_;
  }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  AppTimeNotificationDelegateMock& notification_delegate_mock() {
    return notification_delegate_mock_;
  }

 private:
  TestingProfile profile_;
  AppTimeNotificationDelegateMock notification_delegate_mock_;
  chromeos::app_time::AppServiceWrapper wrapper_{&profile_};
  chromeos::app_time::AppActivityRegistry registry_{
      &wrapper_, &notification_delegate_mock_};

  std::map<chromeos::app_time::AppId,
           std::vector<std::unique_ptr<aura::Window>>>
      windows_;
};

void AppActivityRegistryTest::SetUp() {
  ChromeViewsTestBase::SetUp();
  app_activity_registry().OnAppInstalled(kApp1);
  app_activity_registry().OnAppInstalled(kApp2);
  app_activity_registry().OnAppAvailable(kApp1);
  app_activity_registry().OnAppAvailable(kApp2);
}

aura::Window* AppActivityRegistryTest::CreateWindowForApp(
    const chromeos::app_time::AppId& app_id) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LayerType::LAYER_NOT_DRAWN);

  aura::Window* to_return = window.get();
  windows_[app_id].push_back(std::move(window));
  return to_return;
}

TEST_F(AppActivityRegistryTest, RunningActiveTimeCheck) {
  auto* app1_window = CreateWindowForApp(kApp1);

  base::Time app1_start_time = base::Time::Now();
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(5);
  app_activity_registry().OnAppActive(kApp1, app1_window, app1_start_time);
  task_environment().FastForwardBy(active_time / 2);
  EXPECT_EQ(active_time / 2, app_activity_registry().GetActiveTime(kApp1));
  EXPECT_TRUE(app_activity_registry().IsAppActive(kApp1));

  task_environment().FastForwardBy(active_time / 2);
  base::Time app1_end_time = base::Time::Now();
  app_activity_registry().OnAppInactive(kApp1, app1_window, app1_end_time);
  EXPECT_EQ(active_time, app_activity_registry().GetActiveTime(kApp1));
  EXPECT_FALSE(app_activity_registry().IsAppActive(kApp1));
}

TEST_F(AppActivityRegistryTest, MultipleWindowSameApp) {
  auto* app2_window1 = CreateWindowForApp(kApp2);
  auto* app2_window2 = CreateWindowForApp(kApp2);

  base::TimeDelta app2_active_time = base::TimeDelta::FromMinutes(5);

  app_activity_registry().OnAppActive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  app_activity_registry().OnAppActive(kApp2, app2_window2, base::Time::Now());
  app_activity_registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  app_activity_registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  EXPECT_TRUE(app_activity_registry().IsAppActive(kApp2));

  task_environment().FastForwardBy(app2_active_time / 2);

  // Repeated calls to OnAppInactive shouldn't affect the time calculation.
  app_activity_registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());

  // Mark the application inactive.
  app_activity_registry().OnAppInactive(kApp2, app2_window2, base::Time::Now());

  // There was no interruption in active times. Therefore, the app should
  // be active for the whole 5 minutes.
  EXPECT_EQ(app2_active_time, app_activity_registry().GetActiveTime(kApp2));

  base::TimeDelta app2_inactive_time = base::TimeDelta::FromMinutes(1);

  app_activity_registry().OnAppActive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  app_activity_registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_inactive_time);
  EXPECT_FALSE(app_activity_registry().IsAppActive(kApp2));

  app_activity_registry().OnAppActive(kApp2, app2_window2, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  app_activity_registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  EXPECT_TRUE(app_activity_registry().IsAppActive(kApp2));

  app_activity_registry().OnAppInactive(kApp2, app2_window2, base::Time::Now());
  EXPECT_FALSE(app_activity_registry().IsAppActive(kApp2));

  EXPECT_EQ(app2_active_time * 2, app_activity_registry().GetActiveTime(kApp2));
}

TEST_F(AppActivityRegistryTest, AppTimeLimitReachedActiveApp) {
  base::Time start = base::Time::Now();
  // Set the time limit for kApp1 to be 10 minutes.
  bool state_changed = app_activity_registry().SetAppTimeLimitForTest(
      kApp1, base::TimeDelta::FromMinutes(10), start);

  EXPECT_FALSE(state_changed);
  EXPECT_EQ(app_activity_registry().GetAppState(kApp1),
            chromeos::app_time::AppState::kAvailable);

  auto* app1_window = CreateWindowForApp(kApp1);

  app_activity_registry().OnAppActive(kApp1, app1_window, start);

  // Expect 5 minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(
                  kApp1, chromeos::app_time::AppNotification::kFiveMinutes))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5),
            app_activity_registry().GetActiveTime(kApp1));
  EXPECT_TRUE(app_activity_registry().IsAppActive(kApp1));

  // Expect One minute left notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(
                  kApp1, chromeos::app_time::AppNotification::kOneMinute))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(4));
  EXPECT_EQ(base::TimeDelta::FromMinutes(9),
            app_activity_registry().GetActiveTime(kApp1));

  // Expect time limit reached notification.
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp1, chromeos::app_time::AppNotification::kTimeLimitReached))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10),
            app_activity_registry().GetActiveTime(kApp1));

  EXPECT_EQ(app_activity_registry().GetAppState(kApp1),
            chromeos::app_time::AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, SkippedFiveMinuteNotification) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  bool state_changed = app_activity_registry().SetAppTimeLimitForTest(
      kApp1, base::TimeDelta::FromMinutes(25), start);
  EXPECT_FALSE(state_changed);

  auto* app1_window = CreateWindowForApp(kApp1);
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(10);
  app_activity_registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(active_time);

  app_activity_registry().SetAppTimeLimitForTest(
      kApp1, base::TimeDelta::FromMinutes(14), start + active_time);

  // Notice that the 5 minute notification is jumped.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(
                  kApp1, chromeos::app_time::AppNotification::kOneMinute))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(3));
}

TEST_F(AppActivityRegistryTest, SkippedAllNotifications) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  app_activity_registry().SetAppTimeLimitForTest(
      kApp1, base::TimeDelta::FromMinutes(25), start);

  auto* app1_window = CreateWindowForApp(kApp1);
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(10);
  app_activity_registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(active_time);

  // Notice that the 5 minute and 1 minute notifications are jumped.

  bool state_changed = app_activity_registry().SetAppTimeLimitForTest(
      kApp1, base::TimeDelta::FromMinutes(5), start + active_time);
  EXPECT_TRUE(state_changed);
  EXPECT_EQ(app_activity_registry().GetAppState(kApp1),
            chromeos::app_time::AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, BlockedAppSetAvailable) {
  base::Time start = base::Time::Now();

  const base::TimeDelta kTenMinutes = base::TimeDelta::FromMinutes(10);
  app_activity_registry().SetAppTimeLimitForTest(kApp1, kTenMinutes, start);

  auto* app1_window = CreateWindowForApp(kApp1);
  app_activity_registry().OnAppActive(kApp1, app1_window, start);

  // There are going to be a bunch of mock notification calls for kFiveMinutes,
  // kOneMinute, and kTimeLimitReached. They have already been tested in the
  // other tests. Let's igonre them.
  task_environment().FastForwardBy(kTenMinutes);

  EXPECT_EQ(app_activity_registry().GetAppState(kApp1),
            chromeos::app_time::AppState::kLimitReached);

  const base::TimeDelta new_active_time = base::TimeDelta::FromMinutes(20);
  bool state_changed = app_activity_registry().SetAppTimeLimitForTest(
      kApp1, new_active_time, start + kTenMinutes);
  EXPECT_TRUE(state_changed);
  EXPECT_EQ(app_activity_registry().GetAppState(kApp1),
            chromeos::app_time::AppState::kAvailable);
}

TEST_F(AppActivityRegistryTest, ResetTimeReached) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kTenMinutes = base::TimeDelta::FromMinutes(10);

  const base::TimeDelta kApp1Limit = kTenMinutes;
  const base::TimeDelta kApp2Limit = base::TimeDelta::FromMinutes(20);
  app_activity_registry().SetAppTimeLimitForTest(kApp1, kApp1Limit, start);
  app_activity_registry().SetAppTimeLimitForTest(kApp2, kApp2Limit, start);

  auto* app1_window = CreateWindowForApp(kApp1);
  auto* app2_window = CreateWindowForApp(kApp2);
  app_activity_registry().OnAppActive(kApp1, app1_window, start);
  app_activity_registry().OnAppActive(kApp2, app2_window, start);

  task_environment().FastForwardBy(kTenMinutes);

  // App 1's time limit has been reached.
  EXPECT_TRUE(app_activity_registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, app_activity_registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(app_activity_registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, app_activity_registry().GetActiveTime(kApp2));

  // Reset time has been reached.
  app_activity_registry().OnResetTimeReached(start + kTenMinutes);
  EXPECT_FALSE(app_activity_registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0),
            app_activity_registry().GetActiveTime(kApp1));
  EXPECT_FALSE(app_activity_registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0),
            app_activity_registry().GetActiveTime(kApp2));

  // Now make sure that the timers have been scheduled appropriately.
  app_activity_registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(kTenMinutes);

  EXPECT_TRUE(app_activity_registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, app_activity_registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(app_activity_registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, app_activity_registry().GetActiveTime(kApp2));

  // Now let's make sure
  task_environment().FastForwardBy(kTenMinutes);
  EXPECT_TRUE(app_activity_registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kApp2Limit, app_activity_registry().GetActiveTime(kApp2));
}
