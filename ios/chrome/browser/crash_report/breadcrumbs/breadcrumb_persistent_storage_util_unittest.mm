// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/web/public/browser_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageFilePath;

using breadcrumb_persistent_storage_util::
    GetBreadcrumbPersistentStorageTempFilePath;

// Test fixture to test BreadcrumbPersistentStorageUtil.
typedef ChromeWebTest BreadcrumbPersistentStorageUtilTest;

// Tests that the storage paths are unique for different BrowserStates.
TEST_F(BreadcrumbPersistentStorageUtilTest, UniqueStorage) {
  base::FilePath path1 =
      GetBreadcrumbPersistentStorageFilePath(GetBrowserState());
  base::FilePath tempFilePath1 =
      GetBreadcrumbPersistentStorageTempFilePath(GetBrowserState());
  // Verify that the temp file path is different.
  EXPECT_NE(path1, tempFilePath1);

  std::unique_ptr<TestChromeBrowserState> local_browser_state =
      TestChromeBrowserState::Builder().Build();
  base::FilePath path2 =
      GetBreadcrumbPersistentStorageFilePath(local_browser_state.get());
  base::FilePath tempFilePath2 =
      GetBreadcrumbPersistentStorageTempFilePath(local_browser_state.get());

  EXPECT_NE(path1, path2);
  EXPECT_NE(tempFilePath1, tempFilePath2);
}

// Tests that the BrowserState returned by
// |BrowserState::GetOffTheRecordChromeBrowserState| does not share a storage
// path with the original BrowserState.
TEST_F(BreadcrumbPersistentStorageUtilTest, UniqueIncognitoStorage) {
  base::FilePath path1 =
      GetBreadcrumbPersistentStorageFilePath(GetBrowserState());
  base::FilePath tempFilePath1 =
      GetBreadcrumbPersistentStorageTempFilePath(GetBrowserState());

  ChromeBrowserState* off_the_record_browser_state =
      chrome_browser_state_->GetOffTheRecordChromeBrowserState();
  base::FilePath path2 =
      GetBreadcrumbPersistentStorageFilePath(off_the_record_browser_state);
  base::FilePath tempFilePath2 =
      GetBreadcrumbPersistentStorageTempFilePath(off_the_record_browser_state);

  EXPECT_NE(path1, path2);
  EXPECT_NE(tempFilePath1, tempFilePath2);
}
