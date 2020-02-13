// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class CannedBrowsingDataIndexedDBHelperTest : public testing::Test {
 public:
  content::StoragePartition* StoragePartition() {
    return content::BrowserContext::GetDefaultStoragePartition(&profile_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(CannedBrowsingDataIndexedDBHelperTest, Empty) {
  const url::Origin origin = url::Origin::Create(GURL("http://host1:1/"));
  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(StoragePartition()));

  ASSERT_TRUE(helper->empty());
  helper->Add(origin);
  ASSERT_FALSE(helper->empty());
  helper->Reset();
  ASSERT_TRUE(helper->empty());
}

TEST_F(CannedBrowsingDataIndexedDBHelperTest, Delete) {
  const url::Origin origin1 = url::Origin::Create(GURL("http://host1:9000"));
  const url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(StoragePartition()));

  EXPECT_TRUE(helper->empty());
  helper->Add(origin1);
  helper->Add(origin2);
  EXPECT_EQ(2u, helper->GetCount());
  base::RunLoop loop;
  helper->DeleteIndexedDB(origin2,
                          base::BindLambdaForTesting([&](bool success) {
                            EXPECT_TRUE(success);
                            loop.Quit();
                          }));
  loop.Run();
  EXPECT_EQ(1u, helper->GetCount());
}

TEST_F(CannedBrowsingDataIndexedDBHelperTest, IgnoreExtensionsAndDevTools) {
  const url::Origin origin1 = url::Origin::Create(
      GURL("chrome-extension://abcdefghijklmnopqrstuvwxyz/"));
  const url::Origin origin2 =
      url::Origin::Create(GURL("devtools://abcdefghijklmnopqrstuvwxyz/"));

  scoped_refptr<CannedBrowsingDataIndexedDBHelper> helper(
      new CannedBrowsingDataIndexedDBHelper(StoragePartition()));

  ASSERT_TRUE(helper->empty());
  helper->Add(origin1);
  ASSERT_TRUE(helper->empty());
  helper->Add(origin2);
  ASSERT_TRUE(helper->empty());
}

}  // namespace
