// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper_browsertest.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
using TestCompletionCallback =
    BrowsingDataHelperCallback<content::StorageUsageInfo>;

class BrowsingDataIndexedDBHelperTest : public InProcessBrowserTest {
 public:
  content::StoragePartition* StoragePartition() {
    return content::BrowserContext::GetDefaultStoragePartition(
        browser()->profile());
  }
};

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedAddIndexedDB) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(StoragePartition()));
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(2U, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin1, info->origin.GetURL());
  info++;
  EXPECT_EQ(origin2, info->origin.GetURL());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataIndexedDBHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(StoragePartition()));
  helper->Add(url::Origin::Create(origin));
  helper->Add(url::Origin::Create(origin));

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(1U, result.size());
  EXPECT_EQ(origin, result.begin()->origin.GetURL());
}
}  // namespace
