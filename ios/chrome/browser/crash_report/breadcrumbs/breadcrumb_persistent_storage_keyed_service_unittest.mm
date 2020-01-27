// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageFilePath;

namespace {
// Creates a new BreadcrumbPersistentStorageKeyedService for |browser_state|.
std::unique_ptr<KeyedService> BuildBreadcrumbPersistentStorageKeyedService(
    web::BrowserState* browser_state) {
  return std::make_unique<BreadcrumbPersistentStorageKeyedService>(
      ChromeBrowserState::FromBrowserState(browser_state));
}
}

class BreadcrumbPersistentStorageKeyedServiceTest : public PlatformTest {
 protected:
  BreadcrumbPersistentStorageKeyedServiceTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    EXPECT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    base::FilePath directory_name = scoped_temp_directory_.GetPath();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(directory_name);
    test_cbs_builder.AddTestingFactory(
        BreadcrumbPersistentStorageKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildBreadcrumbPersistentStorageKeyedService));
    chrome_browser_state_ = test_cbs_builder.Build();

    persistent_storage_ = static_cast<BreadcrumbPersistentStorageKeyedService*>(
        BreadcrumbPersistentStorageKeyedServiceFactory::GetForBrowserState(
            chrome_browser_state_.get()));
  }
  ~BreadcrumbPersistentStorageKeyedServiceTest() override = default;

  void TearDown() override {
    persistent_storage_->ObserveBreadcrumbManager(nullptr);
    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::ScopedTempDir scoped_temp_directory_;
  BreadcrumbManager breadcrumb_manager_;
  BreadcrumbPersistentStorageKeyedService* persistent_storage_;
};

// Ensures that events logged after a BreadcrumbManager is already being
// observed are persisted.
TEST_F(BreadcrumbPersistentStorageKeyedServiceTest, PersistMessages) {
  persistent_storage_->ObserveBreadcrumbManager(&breadcrumb_manager_);
  breadcrumb_manager_.AddEvent("event");

  auto events = persistent_storage_->GetStoredEvents();
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("event"));
}

// Ensures that events logged before a BreadcrumbManager is being observed
// are persisted.
TEST_F(BreadcrumbPersistentStorageKeyedServiceTest, PersistExistingMessages) {
  breadcrumb_manager_.AddEvent("event");
  persistent_storage_->ObserveBreadcrumbManager(&breadcrumb_manager_);

  auto events = persistent_storage_->GetStoredEvents();
  ASSERT_EQ(1ul, events.size());
  EXPECT_NE(std::string::npos, events.front().find("event"));
}

// Tests that calling |ObserveBreadcrumbManager| with a null manager removes the
// contents of the persistent storage file.
TEST_F(BreadcrumbPersistentStorageKeyedServiceTest, DeletePersistentStorage) {
  breadcrumb_manager_.AddEvent("event");
  persistent_storage_->ObserveBreadcrumbManager(&breadcrumb_manager_);
  persistent_storage_->ObserveBreadcrumbManager(/*manager=*/nullptr);

  int64_t file_size = -1;
  ASSERT_TRUE(base::GetFileSize(
      GetBreadcrumbPersistentStorageFilePath(chrome_browser_state_.get()),
      &file_size));
  EXPECT_EQ(0, file_size);
}
