// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"

#include <map>
#include <memory>
#include <vector>

#include "base/optional.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_test_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_notification_delegate.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

namespace chromeos {
namespace app_time {

namespace {

const AppId kApp1(apps::mojom::AppType::kArc, "1");
const AppId kApp2(apps::mojom::AppType::kWeb, "3");

class AppTimeNotificationDelegateMock : public AppTimeNotificationDelegate {
 public:
  AppTimeNotificationDelegateMock() = default;
  AppTimeNotificationDelegateMock(const AppTimeNotificationDelegateMock&) =
      delete;
  AppTimeNotificationDelegateMock& operator=(
      const AppTimeNotificationDelegateMock&) = delete;

  ~AppTimeNotificationDelegateMock() = default;

  MOCK_METHOD3(ShowAppTimeLimitNotification,
               void(const chromeos::app_time::AppId&,
                    const base::Optional<base::TimeDelta>&,
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

  aura::Window* CreateWindowForApp(const AppId& app_id);

  void SetAppLimit(const AppId& app_id,
                   const base::Optional<AppLimit>& app_limit);

  AppActivityRegistry& registry() { return registry_; }
  AppActivityRegistry::TestApi& registry_test() { return registry_test_; }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }
  AppTimeNotificationDelegateMock& notification_delegate_mock() {
    return notification_delegate_mock_;
  }

 private:
  TestingProfile profile_;
  AppTimeNotificationDelegateMock notification_delegate_mock_;
  AppServiceWrapper wrapper_{&profile_};
  AppActivityRegistry registry_{&wrapper_, &notification_delegate_mock_};
  AppActivityRegistry::TestApi registry_test_{&registry_};

  std::map<AppId, std::vector<std::unique_ptr<aura::Window>>> windows_;
};

void AppActivityRegistryTest::SetUp() {
  ChromeViewsTestBase::SetUp();
  registry().OnAppInstalled(GetChromeAppId());
  registry().OnAppInstalled(kApp1);
  registry().OnAppInstalled(kApp2);
  registry().OnAppAvailable(GetChromeAppId());
  registry().OnAppAvailable(kApp1);
  registry().OnAppAvailable(kApp2);
}

aura::Window* AppActivityRegistryTest::CreateWindowForApp(const AppId& app_id) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LayerType::LAYER_NOT_DRAWN);

  aura::Window* to_return = window.get();
  windows_[app_id].push_back(std::move(window));
  return to_return;
}

void AppActivityRegistryTest::SetAppLimit(
    const AppId& app_id,
    const base::Optional<AppLimit>& app_limit) {
  registry_.SetAppLimit(app_id, app_limit);
  task_environment_.RunUntilIdle();
}

TEST_F(AppActivityRegistryTest, RunningActiveTimeCheck) {
  auto* app1_window = CreateWindowForApp(kApp1);

  base::Time app1_start_time = base::Time::Now();
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(5);
  registry().OnAppActive(kApp1, app1_window, app1_start_time);
  task_environment().FastForwardBy(active_time / 2);
  EXPECT_EQ(active_time / 2, registry().GetActiveTime(kApp1));
  EXPECT_TRUE(registry().IsAppActive(kApp1));

  task_environment().FastForwardBy(active_time / 2);
  base::Time app1_end_time = base::Time::Now();
  registry().OnAppInactive(kApp1, app1_window, app1_end_time);
  EXPECT_EQ(active_time, registry().GetActiveTime(kApp1));
  EXPECT_FALSE(registry().IsAppActive(kApp1));
}

TEST_F(AppActivityRegistryTest, MultipleWindowSameApp) {
  auto* app2_window1 = CreateWindowForApp(kApp2);
  auto* app2_window2 = CreateWindowForApp(kApp2);

  base::TimeDelta app2_active_time = base::TimeDelta::FromMinutes(5);

  registry().OnAppActive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  registry().OnAppActive(kApp2, app2_window2, base::Time::Now());
  registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(kApp2));

  task_environment().FastForwardBy(app2_active_time / 2);

  // Repeated calls to OnAppInactive shouldn't affect the time calculation.
  registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());

  // Mark the application inactive.
  registry().OnAppInactive(kApp2, app2_window2, base::Time::Now());

  // There was no interruption in active times. Therefore, the app should
  // be active for the whole 5 minutes.
  EXPECT_EQ(app2_active_time, registry().GetActiveTime(kApp2));

  base::TimeDelta app2_inactive_time = base::TimeDelta::FromMinutes(1);

  registry().OnAppActive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  task_environment().FastForwardBy(app2_inactive_time);
  EXPECT_FALSE(registry().IsAppActive(kApp2));

  registry().OnAppActive(kApp2, app2_window2, base::Time::Now());
  task_environment().FastForwardBy(app2_active_time / 2);

  registry().OnAppInactive(kApp2, app2_window1, base::Time::Now());
  EXPECT_TRUE(registry().IsAppActive(kApp2));

  registry().OnAppInactive(kApp2, app2_window2, base::Time::Now());
  EXPECT_FALSE(registry().IsAppActive(kApp2));

  EXPECT_EQ(app2_active_time * 2, registry().GetActiveTime(kApp2));
}

TEST_F(AppActivityRegistryTest, AppTimeLimitReachedActiveApp) {
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 10 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(10), start);
  SetAppLimit(kApp1, limit);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);

  auto* app1_window = CreateWindowForApp(kApp1);

  registry().OnAppActive(kApp1, app1_window, start);

  // Expect 5 minute left notification.
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp1, testing::_, chromeos::app_time::AppNotification::kFiveMinutes))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_TRUE(registry().IsAppActive(kApp1));

  // Expect One minute left notification.
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp1, testing::_, chromeos::app_time::AppNotification::kOneMinute))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(4));
  EXPECT_EQ(base::TimeDelta::FromMinutes(9), registry().GetActiveTime(kApp1));

  // Expect time limit reached notification.
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(
                  kApp1, testing::_,
                  chromeos::app_time::AppNotification::kTimeLimitReached))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10), registry().GetActiveTime(kApp1));

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, SkippedFiveMinuteNotification) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(25), start);
  SetAppLimit(kApp1, limit);

  auto* app1_window = CreateWindowForApp(kApp1);
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(10);
  registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(active_time);

  const AppLimit new_limit(AppRestriction::kTimeLimit,
                           base::TimeDelta::FromMinutes(14),
                           start + active_time);
  SetAppLimit(kApp1, new_limit);

  // Notice that the 5 minute notification is jumped.
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp1, testing::_, chromeos::app_time::AppNotification::kOneMinute))
      .Times(1);
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(3));
}

TEST_F(AppActivityRegistryTest, SkippedAllNotifications) {
  // The application is inactive when the time limit is reached.
  base::Time start = base::Time::Now();

  // Set the time limit for kApp1 to be 25 minutes.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(25), start);
  SetAppLimit(kApp1, limit);

  auto* app1_window = CreateWindowForApp(kApp1);
  base::TimeDelta active_time = base::TimeDelta::FromMinutes(10);
  registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(active_time);

  // Notice that the 5 minute and 1 minute notifications are jumped.
  const AppLimit new_limit(AppRestriction::kTimeLimit,
                           base::TimeDelta::FromMinutes(5),
                           start + active_time);
  SetAppLimit(kApp1, new_limit);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);
}

TEST_F(AppActivityRegistryTest, BlockedAppSetAvailable) {
  base::Time start = base::Time::Now();

  const base::TimeDelta kTenMinutes = base::TimeDelta::FromMinutes(10);
  const AppLimit limit(AppRestriction::kTimeLimit, kTenMinutes, start);
  SetAppLimit(kApp1, limit);

  auto* app1_window = CreateWindowForApp(kApp1);
  registry().OnAppActive(kApp1, app1_window, start);

  // There are going to be a bunch of mock notification calls for kFiveMinutes,
  // kOneMinute, and kTimeLimitReached. They have already been tested in the
  // other tests. Let's igonre them.
  task_environment().FastForwardBy(kTenMinutes);

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kLimitReached);

  const AppLimit new_limit(AppRestriction::kTimeLimit,
                           base::TimeDelta::FromMinutes(20),
                           start + kTenMinutes);
  SetAppLimit(kApp1, new_limit);
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);
}

TEST_F(AppActivityRegistryTest, ResetTimeReached) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kTenMinutes = base::TimeDelta::FromMinutes(10);

  const AppLimit limit1(AppRestriction::kTimeLimit, kTenMinutes, start);
  const AppLimit limit2(AppRestriction::kTimeLimit,
                        base::TimeDelta::FromMinutes(20), start);
  const std::map<AppId, AppLimit> limits{{kApp1, limit1}, {kApp2, limit2}};
  registry().UpdateAppLimits(limits);

  auto* app1_window = CreateWindowForApp(kApp1);
  auto* app2_window = CreateWindowForApp(kApp2);
  registry().OnAppActive(kApp1, app1_window, start);
  registry().OnAppActive(kApp2, app2_window, start);

  task_environment().FastForwardBy(kTenMinutes);

  // App 1's time limit has been reached.
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp2));

  // Reset time has been reached.
  registry().OnResetTimeReached(start + kTenMinutes);
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), registry().GetActiveTime(kApp1));
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), registry().GetActiveTime(kApp2));

  // Now make sure that the timers have been scheduled appropriately.
  registry().OnAppActive(kApp1, app1_window, start);

  task_environment().FastForwardBy(kTenMinutes);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp1));

  // App 2 is still active.
  EXPECT_FALSE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(kTenMinutes, registry().GetActiveTime(kApp2));

  // Now let's make sure
  task_environment().FastForwardBy(kTenMinutes);
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_EQ(*limit2.daily_limit(), registry().GetActiveTime(kApp2));
}

TEST_F(AppActivityRegistryTest, SharedTimeLimitForChromeAndWebApps) {
  base::Time start = base::Time::Now();
  const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
  const base::TimeDelta kHalfHour = base::TimeDelta::FromMinutes(30);

  const AppId kChromeAppId = GetChromeAppId();

  const AppLimit limit1(AppRestriction::kTimeLimit, kOneHour, start);
  const std::map<AppId, AppLimit> limits{{kChromeAppId, limit1}};

  registry().UpdateAppLimits(limits);

  auto* app2_window = CreateWindowForApp(kApp2);

  // Make chrome active for 30 minutes.
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kActive, start);
  task_environment().FastForwardBy(kHalfHour);
  registry().OnChromeAppActivityChanged(ChromeAppActivityState::kInactive,
                                        start + kHalfHour);

  // Expect that the active running time for app2 has been updated.
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(kChromeAppId));
  EXPECT_EQ(kHalfHour, registry().GetActiveTime(kApp2));

  // Make |kApp2| active for 30 minutes. Expect that it reaches its time limit.
  registry().OnAppActive(kApp2, app2_window, start + kHalfHour);
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp2, testing::_, chromeos::app_time::AppNotification::kFiveMinutes))
      .Times(1);
  EXPECT_CALL(
      notification_delegate_mock(),
      ShowAppTimeLimitNotification(
          kApp2, testing::_, chromeos::app_time::AppNotification::kOneMinute))
      .Times(1);
  EXPECT_CALL(notification_delegate_mock(),
              ShowAppTimeLimitNotification(
                  kApp2, testing::_,
                  chromeos::app_time::AppNotification::kTimeLimitReached))
      .Times(1);

  task_environment().FastForwardBy(kHalfHour);

  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp2));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kChromeAppId));
}

TEST_F(AppActivityRegistryTest, LimitChangedForActiveApp) {
  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAvailable);

  auto* app1_window = CreateWindowForApp(kApp1);
  base::Time start = base::Time::Now();
  registry().OnAppActive(kApp1, app1_window, start);

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(0), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::nullopt, registry_test().GetAppLimit(kApp1));
  EXPECT_EQ(base::nullopt, registry_test().GetTimeLeft(kApp1));

  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));

  // Limit set for active app.
  const AppLimit limit1(AppRestriction::kTimeLimit,
                        base::TimeDelta::FromMinutes(11), base::Time::Now());
  SetAppLimit(kApp1, limit1);

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(11),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(6),
            registry_test().GetTimeLeft(kApp1));

  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));

  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(11),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(1),
            registry_test().GetTimeLeft(kApp1));

  // Increase the limit.
  const AppLimit limit_increase(AppRestriction::kTimeLimit,
                                base::TimeDelta::FromMinutes(20),
                                base::Time::Now());
  SetAppLimit(kApp1, limit_increase);
  EXPECT_TRUE(registry().IsAppActive(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(20),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(10),
            registry_test().GetTimeLeft(kApp1));

  // Decrease the limit.
  const AppLimit limit_decrease(AppRestriction::kTimeLimit,
                                base::TimeDelta::FromMinutes(5),
                                base::Time::Now());
  SetAppLimit(kApp1, limit_decrease);
  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            registry_test().GetTimeLeft(kApp1));
}

TEST_F(AppActivityRegistryTest, LimitChangesForInactiveApp) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(5), base::Time::Now());
  SetAppLimit(kApp1, limit);

  // Use available limit - app should become paused.
  auto* app1_window = CreateWindowForApp(kApp1);
  registry().OnAppActive(kApp1, app1_window, base::Time::Now());
  task_environment().FastForwardBy(base::TimeDelta::FromMinutes(5));

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            registry_test().GetTimeLeft(kApp1));

  // Decrease limit - app should remain paused.
  const AppLimit decreased_limit(AppRestriction::kTimeLimit,
                                 base::TimeDelta::FromMinutes(3),
                                 base::Time::Now());
  SetAppLimit(kApp1, decreased_limit);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(3),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            registry_test().GetTimeLeft(kApp1));

  // Increase limit - app should become available, but inactive.
  const AppLimit increased_limit(AppRestriction::kTimeLimit,
                                 base::TimeDelta::FromMinutes(10),
                                 base::Time::Now());
  SetAppLimit(kApp1, increased_limit);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppAvailable(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(10),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(5),
            registry_test().GetTimeLeft(kApp1));

  // Decrease limit above used time - app should stay available.
  const AppLimit limit_above_used(AppRestriction::kTimeLimit,
                                  base::TimeDelta::FromMinutes(8),
                                  base::Time::Now());
  SetAppLimit(kApp1, limit_above_used);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppAvailable(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(8),
            registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(3),
            registry_test().GetTimeLeft(kApp1));

  // Decrease limit below below time - app should become unavailabe.
  const AppLimit limit_below_used(AppRestriction::kTimeLimit,
                                  base::TimeDelta::FromMinutes(4),
                                  base::Time::Now());
  SetAppLimit(kApp1, limit_below_used);

  EXPECT_FALSE(registry().IsAppActive(kApp1));
  EXPECT_TRUE(registry().IsAppTimeLimitReached(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(5), registry().GetActiveTime(kApp1));
  EXPECT_EQ(base::TimeDelta::FromMinutes(4),
            *registry_test().GetAppLimit(kApp1)->daily_limit());
  EXPECT_EQ(base::TimeDelta::FromMinutes(0),
            *registry_test().GetTimeLeft(kApp1));
}

TEST_F(AppActivityRegistryTest, RemoveLimitsFromWhitelistedApps) {
  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(5), base::Time::Now());
  SetAppLimit(kApp1, limit);
  SetAppLimit(kApp2, limit);

  AppTimeLimitsWhitelistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToWhitelistAppList(kApp1);

  AppTimeLimitsWhitelistPolicyWrapper wrapper(&builder.value());
  registry().OnTimeLimitWhitelistChanged(wrapper);

  EXPECT_FALSE(registry_test().GetAppLimit(kApp1));

  EXPECT_EQ(registry_test().GetAppLimit(kApp2)->daily_limit(),
            limit.daily_limit());

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAlwaysAvailable);
}

TEST_F(AppActivityRegistryTest, WhitelistedAppsNoLimits) {
  AppTimeLimitsWhitelistPolicyBuilder builder;
  builder.SetUp();
  builder.AppendToWhitelistAppList(kApp1);
  AppTimeLimitsWhitelistPolicyWrapper wrapper(&builder.value());
  registry().OnTimeLimitWhitelistChanged(wrapper);

  // Set initial limit.
  const AppLimit limit(AppRestriction::kTimeLimit,
                       base::TimeDelta::FromMinutes(5), base::Time::Now());
  SetAppLimit(kApp1, limit);
  SetAppLimit(kApp2, limit);

  EXPECT_FALSE(registry_test().GetAppLimit(kApp1));

  EXPECT_EQ(registry_test().GetAppLimit(kApp2)->daily_limit(),
            limit.daily_limit());

  EXPECT_EQ(registry().GetAppState(kApp1), AppState::kAlwaysAvailable);
}

}  // namespace app_time
}  // namespace chromeos
