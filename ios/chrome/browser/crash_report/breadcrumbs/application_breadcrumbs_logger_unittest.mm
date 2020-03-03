// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/application_breadcrumbs_logger.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/metrics/user_metrics_action.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#import "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The particular UserActions used here are not important, but real UserAction
// names are used to prevent a presubmit warning.
const char kUserAction1Name[] = "MobileMenuNewTab";
const char kUserAction2Name[] = "OverscrollActionCloseTab";
// An "InProductHelp.*" user action.
const char kInProductHelpUserActionName[] = "InProductHelp.Dismissed";
}  // namespace

using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

// Test fixture for testing ApplicationBreadcrumbsLogger class.
class ApplicationBreadcrumbsLoggerTest : public PlatformTest {
 protected:
  ApplicationBreadcrumbsLoggerTest()
      : logger_(std::make_unique<ApplicationBreadcrumbsLogger>(
            &breadcrumb_manager_)) {}

  base::test::TaskEnvironment task_environment_;
  BreadcrumbManager breadcrumb_manager_;
  std::unique_ptr<ApplicationBreadcrumbsLogger> logger_;
};

// Tests that a recorded UserAction is logged by the
// ApplicationBreadcrumbsLogger.
TEST_F(ApplicationBreadcrumbsLoggerTest, UserAction) {
  base::RecordAction(base::UserMetricsAction(kUserAction1Name));
  base::RecordAction(base::UserMetricsAction(kUserAction2Name));

  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find(kUserAction1Name));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find(kUserAction2Name));
}

// Tests that not_user_triggered User Action does not show up in breadcrumbs.
TEST_F(ApplicationBreadcrumbsLoggerTest, LogNotUserTriggeredAction) {
  base::RecordAction(base::UserMetricsAction("PageLoad"));

  EXPECT_EQ(0U, breadcrumb_manager_.GetEvents(0).size());
}

// Tests that "InProductHelp" UserActions are not logged by
// ApplicationBreadcrumbsLogger as they are very noisy.
TEST_F(ApplicationBreadcrumbsLoggerTest, SkipInProductHelpUserActions) {
  base::RecordAction(base::UserMetricsAction(kInProductHelpUserActionName));

  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(0ul, events.size());
}

// Tests that memory pressure events are logged by ApplicationBreadcrumbsLogger.
// TODO(crbug.com/1046588): This test is flaky.
TEST_F(ApplicationBreadcrumbsLoggerTest, DISABLED_MemoryPressure) {
  base::MemoryPressureListener::SimulatePressureNotification(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::MemoryPressureListener::SimulatePressureNotification(
      MemoryPressureLevel::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();

  std::list<std::string> events = breadcrumb_manager_.GetEvents(0);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("Moderate"));
  // Ensure UserAction events are labeled as such.
  EXPECT_NE(std::string::npos, events.front().find("Memory Pressure: "));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("Critical"));
}
