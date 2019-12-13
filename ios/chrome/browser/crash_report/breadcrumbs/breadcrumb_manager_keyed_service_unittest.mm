// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"

#include "base/time/time.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Creates a new BreadcrumbManagerKeyedService for |browser_state|.
std::unique_ptr<KeyedService> BuildBreadcrumbManagerKeyedService(
    web::BrowserState* browser_state) {
  return std::make_unique<BreadcrumbManagerKeyedService>(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
}

// Test fixture for testing BreadcrumbManagerKeyedService class.
class BreadcrumbManagerKeyedServiceTest : public PlatformTest {
 protected:
  BreadcrumbManagerKeyedServiceTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        BreadcrumbManagerKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildBreadcrumbManagerKeyedService));
    chrome_browser_state_ = test_cbs_builder.Build();

    breadcrumb_manager_ = static_cast<BreadcrumbManagerKeyedService*>(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BreadcrumbManagerKeyedService* breadcrumb_manager_;
};

// Tests that an event is logged and returned.
TEST_F(BreadcrumbManagerKeyedServiceTest, AddEvent) {
  std::string event_message = "event";
  breadcrumb_manager_->AddEvent(event_message);
  std::list<std::string> events = breadcrumb_manager_->GetEvents(0);
  ASSERT_EQ(1ul, events.size());
  // Events returned from |GetEvents| will have a timestamp prepended.
  EXPECT_NE(std::string::npos, events.front().find(event_message));
}

// Tests that returned events returned by |GetEvents| are limited by the
// |event_count_limit| parameter.
TEST_F(BreadcrumbManagerKeyedServiceTest, EventCountLimited) {
  breadcrumb_manager_->AddEvent("event1");
  breadcrumb_manager_->AddEvent("event2");
  breadcrumb_manager_->AddEvent("event3");
  breadcrumb_manager_->AddEvent("event4");

  std::list<std::string> events = breadcrumb_manager_->GetEvents(2);
  ASSERT_EQ(2ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("event3"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event4"));
}

// Tests that old events are dropped.
TEST_F(BreadcrumbManagerKeyedServiceTest, OldEventsDropped) {
  // Log an event from one and two hours ago.
  breadcrumb_manager_->AddEvent("event1");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_->AddEvent("event2");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));

  // Log three events separated by three minutes to ensure they receive their
  // own event bucket. Otherwise, some old events may be returned to ensure a
  // minimum number of available events. See |MinimumEventsReturned| test below.
  breadcrumb_manager_->AddEvent("event3");
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(3));
  breadcrumb_manager_->AddEvent("event4");
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(3));
  breadcrumb_manager_->AddEvent("event5");

  std::list<std::string> events = breadcrumb_manager_->GetEvents(0);
  ASSERT_EQ(3ul, events.size());
  // Validate the three most recent events are the ones which were returned.
  EXPECT_NE(std::string::npos, events.front().find("event3"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event4"));
  events.pop_front();
  EXPECT_NE(std::string::npos, events.front().find("event5"));
}

// Tests that expired events are returned if not enough new events exist.
TEST_F(BreadcrumbManagerKeyedServiceTest, MinimumEventsReturned) {
  // Log an event from one and two hours ago.
  breadcrumb_manager_->AddEvent("event1");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_->AddEvent("event2");
  task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
  breadcrumb_manager_->AddEvent("event3");

  std::list<std::string> events = breadcrumb_manager_->GetEvents(0);
  EXPECT_EQ(2ul, events.size());
}

// Tests that events logged to Normal and OffTheRecord BrowserStates are
// seperately identifiable.
TEST_F(BreadcrumbManagerKeyedServiceTest, EventsLabeledWithBrowserState) {
  breadcrumb_manager_->AddEvent("event");
  std::string event = breadcrumb_manager_->GetEvents(0).front();
  // Event should indicate it was logged from a "Normal" browser state.
  EXPECT_NE(std::string::npos, event.find(" N "));

  ios::ChromeBrowserState* off_the_record_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();

  BreadcrumbManagerKeyedService* off_the_record_breadcrumb_manager =
      static_cast<BreadcrumbManagerKeyedService*>(
          BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
              off_the_record_browser_state));
  off_the_record_breadcrumb_manager->AddEvent("event");
  std::string off_the_record_event =
      off_the_record_breadcrumb_manager->GetEvents(0).front();
  // Event should indicate it was logged from an off the record "Incognito"
  // browser state.
  EXPECT_NE(std::string::npos, off_the_record_event.find(" I "));

  EXPECT_STRNE(event.c_str(), off_the_record_event.c_str());
}
