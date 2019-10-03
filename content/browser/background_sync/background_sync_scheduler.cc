// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_scheduler.h"

#include "base/memory/scoped_refptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace {

const char kBackgroundSyncSchedulerKey[] = "background-sync-scheduler";

}  // namespace

namespace content {

using DelayedProcessingInfo =
    std::map<StoragePartition*, std::unique_ptr<base::OneShotTimer>>;

// static
BackgroundSyncScheduler* BackgroundSyncScheduler::GetFor(
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  if (!browser_context->GetUserData(kBackgroundSyncSchedulerKey)) {
    scoped_refptr<BackgroundSyncScheduler> scheduler =
        base::MakeRefCounted<BackgroundSyncScheduler>();
    browser_context->SetUserData(
        kBackgroundSyncSchedulerKey,
        std::make_unique<base::UserDataAdapter<BackgroundSyncScheduler>>(
            scheduler.get()));
  }

  return base::UserDataAdapter<BackgroundSyncScheduler>::Get(
      browser_context, kBackgroundSyncSchedulerKey);
}

BackgroundSyncScheduler::BackgroundSyncScheduler() = default;

BackgroundSyncScheduler::~BackgroundSyncScheduler() {
  for (auto& one_shot_processing_info : delayed_processing_info_one_shot_)
    one_shot_processing_info.second->Stop();

  for (auto& periodic_processing_info : delayed_processing_info_one_shot_)
    periodic_processing_info.second->Stop();
}

void BackgroundSyncScheduler::ScheduleDelayedProcessing(
    StoragePartition* storage_partition,
    blink::mojom::BackgroundSyncType sync_type,
    base::TimeDelta delay,
    base::OnceClosure delayed_task) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(storage_partition);

  auto& delayed_processing_info = GetDelayedProcessingInfo(sync_type);
  delayed_processing_info.emplace(storage_partition,
                                  std::make_unique<base::OneShotTimer>());

  if (!delay.is_zero() && !delay.is_max()) {
    delayed_processing_info[storage_partition]->Start(FROM_HERE, delay,
                                                      std::move(delayed_task));
  }

  // TODO(crbug.com/996166) Move logic to schedule a browser wakeup task on
  // Android from BackgroundSycnProxy to here.
}

void BackgroundSyncScheduler::CancelDelayedProcessing(
    StoragePartition* storage_partition,
    blink::mojom::BackgroundSyncType sync_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(storage_partition);

  auto& delayed_processing_info = GetDelayedProcessingInfo(sync_type);
  delayed_processing_info.erase(storage_partition);

  // TODO(crbug.com/996166) Move logic to cancel a browser wakeup task on
  // Android from BackgroundSycnProxy to here.
}

DelayedProcessingInfo& BackgroundSyncScheduler::GetDelayedProcessingInfo(
    blink::mojom::BackgroundSyncType sync_type) {
  if (sync_type == blink::mojom::BackgroundSyncType::ONE_SHOT)
    return delayed_processing_info_one_shot_;
  else
    return delayed_processing_info_one_shot_;
}

}  // namespace content
