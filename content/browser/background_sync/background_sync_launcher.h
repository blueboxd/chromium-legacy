// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_

#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#endif

#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

namespace content {

class BrowserContext;
class StoragePartition;

class CONTENT_EXPORT BackgroundSyncLauncher {
 public:
  static BackgroundSyncLauncher* Get();
  static void GetSoonestWakeupDelta(
      BrowserContext* browser_context,
      base::OnceCallback<void(base::TimeDelta)> callback);
#if defined(OS_ANDROID)
  static void FireBackgroundSyncEvents(
      BrowserContext* browser_context,
      blink::mojom::BackgroundSyncType sync_type,
      const base::android::JavaParamRef<jobject>& j_runnable);
#endif

 private:
  friend struct base::LazyInstanceTraitsBase<BackgroundSyncLauncher>;
  friend class BackgroundSyncLauncherTest;

  // Constructor and destructor marked private to enforce singleton.
  BackgroundSyncLauncher();
  ~BackgroundSyncLauncher();

  void GetSoonestWakeupDeltaImpl(
      BrowserContext* browser_context,
      base::OnceCallback<void(base::TimeDelta)> callback);
  void GetSoonestWakeupDeltaForStoragePartition(
      base::OnceClosure done_closure,
      StoragePartition* storage_partition);
  void SendSoonestWakeupDelta(
      base::OnceCallback<void(base::TimeDelta)> callback);

  base::TimeDelta soonest_wakeup_delta_ = base::TimeDelta::Max();
  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncLauncher);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_LAUNCHER_H_