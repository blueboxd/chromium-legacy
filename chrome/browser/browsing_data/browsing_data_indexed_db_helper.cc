// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"

using content::BrowserThread;
using content::StorageUsageInfo;

BrowsingDataIndexedDBHelper::BrowsingDataIndexedDBHelper(
    content::StoragePartition* storage_partition)
    : storage_partition_(storage_partition) {
  DCHECK(storage_partition_);
}

BrowsingDataIndexedDBHelper::~BrowsingDataIndexedDBHelper() {}

void BrowsingDataIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());
  storage_partition_->GetIndexedDBControl().GetUsage(
      base::BindOnce(&BrowsingDataIndexedDBHelper::IndexedDBUsageInfoReceived,
                     base::WrapRefCounted(this), std::move(callback)));
}

void BrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  storage_partition_->GetIndexedDBControl().DeleteForOrigin(
      origin, std::move(callback));
}

void BrowsingDataIndexedDBHelper::IndexedDBUsageInfoReceived(
    FetchCallback callback,
    std::vector<storage::mojom::IndexedDBStorageUsageInfoPtr> origins) {
  DCHECK(!callback.is_null());
  std::list<content::StorageUsageInfo> result;
  for (const auto& origin_usage : origins) {
    if (!BrowsingDataHelper::HasWebScheme(origin_usage->origin.GetURL()))
      continue;  // Non-websafe state is not considered browsing data.
    result.push_back(StorageUsageInfo(origin_usage->origin,
                                      origin_usage->size_in_bytes,
                                      origin_usage->last_modified_time));
  }
  std::move(callback).Run(std::move(result));
}

CannedBrowsingDataIndexedDBHelper::CannedBrowsingDataIndexedDBHelper(
    content::StoragePartition* storage_partition)
    : BrowsingDataIndexedDBHelper(storage_partition) {}

CannedBrowsingDataIndexedDBHelper::~CannedBrowsingDataIndexedDBHelper() {}

void CannedBrowsingDataIndexedDBHelper::Add(const url::Origin& origin) {
  if (!BrowsingDataHelper::HasWebScheme(origin.GetURL()))
    return;  // Non-websafe state is not considered browsing data.

  pending_origins_.insert(origin);
}

void CannedBrowsingDataIndexedDBHelper::Reset() {
  pending_origins_.clear();
}

bool CannedBrowsingDataIndexedDBHelper::empty() const {
  return pending_origins_.empty();
}

size_t CannedBrowsingDataIndexedDBHelper::GetCount() const {
  return pending_origins_.size();
}

const std::set<url::Origin>& CannedBrowsingDataIndexedDBHelper::GetOrigins()
    const {
  return pending_origins_;
}

void CannedBrowsingDataIndexedDBHelper::StartFetching(FetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::list<StorageUsageInfo> result;
  for (const auto& origin : pending_origins_)
    result.emplace_back(origin, 0, base::Time());

  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(std::move(callback), result));
}

void CannedBrowsingDataIndexedDBHelper::DeleteIndexedDB(
    const url::Origin& origin,
    base::OnceCallback<void(bool)> callback) {
  pending_origins_.erase(origin);
  BrowsingDataIndexedDBHelper::DeleteIndexedDB(origin, std::move(callback));
}
