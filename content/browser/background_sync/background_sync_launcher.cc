// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_sync/background_sync_launcher.h"

#include <algorithm>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/background_sync_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#endif

namespace content {

namespace {

base::LazyInstance<BackgroundSyncLauncher>::DestructorAtExit
    g_background_sync_launcher = LAZY_INSTANCE_INITIALIZER;

unsigned int GetStoragePartitionCount(BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  int num_partitions = 0;
  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(
          [](int* num_partitions, StoragePartition* storage_partition) {
            (*num_partitions)++;
          },
          &num_partitions));

  // It's valid for a profile to not have any storage partitions. This DCHECK
  // is to ensure that we're not waking up Chrome for no reason, because that's
  // expensive and unnecessary.
  DCHECK(num_partitions);

  return num_partitions;
}

}  // namespace

// static
BackgroundSyncLauncher* BackgroundSyncLauncher::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return g_background_sync_launcher.Pointer();
}

// static
void BackgroundSyncLauncher::GetSoonestWakeupDelta(
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Get()->GetSoonestWakeupDeltaImpl(browser_context, std::move(callback));
}

// static
#if defined(OS_ANDROID)
void BackgroundSyncLauncher::FireBackgroundSyncEvents(
    BrowserContext* browser_context,
    blink::mojom::BackgroundSyncType sync_type,
    const base::android::JavaParamRef<jobject>& j_runnable) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(browser_context);

  base::RepeatingClosure done_closure = base::BarrierClosure(
      GetStoragePartitionCount(browser_context),
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_runnable)));

  BrowserContext::ForEachStoragePartition(
      browser_context, base::BindRepeating(
                           [](blink::mojom::BackgroundSyncType sync_type,
                              base::OnceClosure done_closure,
                              StoragePartition* storage_partition) {
                             BackgroundSyncContext* sync_context =
                                 storage_partition->GetBackgroundSyncContext();
                             DCHECK(sync_context);
                             sync_context->FireBackgroundSyncEvents(
                                 sync_type, std::move(done_closure));
                           },
                           sync_type, std::move(done_closure)));
}
#endif

BackgroundSyncLauncher::BackgroundSyncLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

BackgroundSyncLauncher::~BackgroundSyncLauncher() = default;

void BackgroundSyncLauncher::GetSoonestWakeupDeltaImpl(
    BrowserContext* browser_context,
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::RepeatingClosure done_closure = base::BarrierClosure(
      GetStoragePartitionCount(browser_context),
      base::BindOnce(&BackgroundSyncLauncher::SendSoonestWakeupDelta,
                     base::Unretained(this), std::move(callback)));

  soonest_wakeup_delta_ = base::TimeDelta::Max();
  BrowserContext::ForEachStoragePartition(
      browser_context,
      base::BindRepeating(
          &BackgroundSyncLauncher::GetSoonestWakeupDeltaForStoragePartition,
          base::Unretained(this), std::move(done_closure)));
}

void BackgroundSyncLauncher::GetSoonestWakeupDeltaForStoragePartition(
    base::OnceClosure done_closure,
    StoragePartition* storage_partition) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  BackgroundSyncContext* sync_context =
      storage_partition->GetBackgroundSyncContext();
  DCHECK(sync_context);

  sync_context->GetSoonestWakeupDelta(base::BindOnce(
      [](base::OnceClosure done_closure, base::TimeDelta* soonest_wakeup_delta,
         base::TimeDelta wakeup_delta) {
        DCHECK_CURRENTLY_ON(BrowserThread::UI);
        *soonest_wakeup_delta = std::min(*soonest_wakeup_delta, wakeup_delta);
        std::move(done_closure).Run();
      },
      std::move(done_closure), &soonest_wakeup_delta_));
}

void BackgroundSyncLauncher::SendSoonestWakeupDelta(
    base::OnceCallback<void(base::TimeDelta)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(soonest_wakeup_delta_);
}

}  // namespace content