// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/mock_browsing_data_indexed_db_helper.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

MockBrowsingDataIndexedDBHelper::MockBrowsingDataIndexedDBHelper(
    Profile* profile)
    : BrowsingDataIndexedDBHelper(
          content::BrowserContext::GetDefaultStoragePartition(profile)) {}

MockBrowsingDataIndexedDBHelper::~MockBrowsingDataIndexedDBHelper() {
}

void MockBrowsingDataIndexedDBHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  origins_[origin] = false;

  bool success = true;
  std::move(callback).Run(success);
}

void MockBrowsingDataIndexedDBHelper::AddIndexedDBSamples() {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("http://idbhost1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("http://idbhost2:2/"));

  content::StorageUsageInfo info1(kOrigin1, 1, base::Time());
  response_.push_back(info1);
  origins_[kOrigin1] = true;

  content::StorageUsageInfo info2(kOrigin2, 2, base::Time());
  response_.push_back(info2);
  origins_[kOrigin2] = true;
}

void MockBrowsingDataIndexedDBHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockBrowsingDataIndexedDBHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockBrowsingDataIndexedDBHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}
