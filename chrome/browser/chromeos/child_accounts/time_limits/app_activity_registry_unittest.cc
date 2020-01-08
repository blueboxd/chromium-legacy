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

 private:
  TestingProfile profile_;
  chromeos::app_time::AppServiceWrapper wrapper_{&profile_};
  chromeos::app_time::AppActivityRegistry registry_{&wrapper_};

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
