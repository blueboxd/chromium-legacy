// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_observer.h"

#import "base/macros.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
class FakeBreadcrumbManagerObserver : public BreadcrumbManagerObserver {
 public:
  FakeBreadcrumbManagerObserver() {}
  ~FakeBreadcrumbManagerObserver() override = default;

  // BreadcrumbManagerObserver
  void EventAdded(BreadcrumbManagerKeyedService* manager,
                  const std::string& event) override {
    last_received_manager_ = manager;
    last_received_event_ = event;
  }

  BreadcrumbManagerKeyedService* last_received_manager_ = nullptr;
  std::string last_received_event_;

  DISALLOW_COPY_AND_ASSIGN(FakeBreadcrumbManagerObserver);
};

// Creates a new BreadcrumbManagerKeyedService for |browser_state|.
std::unique_ptr<KeyedService> BuildBreadcrumbManagerKeyedService(
    web::BrowserState* browser_state) {
  return std::make_unique<BreadcrumbManagerKeyedService>(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
}

class BreadcrumbManagerObserverTest : public PlatformTest {
 protected:
  BreadcrumbManagerObserverTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        BreadcrumbManagerKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildBreadcrumbManagerKeyedService));
    chrome_browser_state_ = test_cbs_builder.Build();

    manager_ = static_cast<BreadcrumbManagerKeyedService*>(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    manager_->AddObserver(&observer_);
  }

  ~BreadcrumbManagerObserverTest() override {
    manager_->RemoveObserver(&observer_);
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  BreadcrumbManagerKeyedService* manager_;
  FakeBreadcrumbManagerObserver observer_;
};

// Tests that |BreadcrumbManagerObserver::EventAdded| is called when an event to
// added to |manager_|.
TEST_F(BreadcrumbManagerObserverTest, EventAdded) {
  ASSERT_FALSE(observer_.last_received_manager_);
  ASSERT_TRUE(observer_.last_received_event_.empty());

  std::string event = "event";
  manager_->AddEvent(event);

  EXPECT_EQ(manager_, observer_.last_received_manager_);
  // A timestamp will be prepended to the event passed to |AddEvent|.
  EXPECT_NE(std::string::npos, observer_.last_received_event_.find(event));
}
