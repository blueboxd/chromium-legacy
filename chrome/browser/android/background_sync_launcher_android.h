// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_

#include <stdint.h>

#include <set>

#include "base/android/jni_android.h"
#include "base/callback_forward.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/background_sync/background_sync.mojom.h"

// The BackgroundSyncLauncherAndroid singleton owns the Java
// BackgroundSyncLauncher object and is used to register interest in starting
// the browser the next time the device goes online. This class runs on the UI
// thread.
class BackgroundSyncLauncherAndroid {
 public:
  static BackgroundSyncLauncherAndroid* Get();

  // Calculates the soonest wakeup time across all the storage
  // partitions for the non-incognito profile and ensures that the browser
  // is running when the device next goes online after that time has passed.
  // If this time is set to base::TimeDelta::Max() across all storage
  // partitions, the wake-up task is cancelled.
  static void ScheduleBrowserWakeUp(blink::mojom::BackgroundSyncType sync_type);

  // Schedule a background task to bring up Chrome when the device next goes
  // online after |soonest_wakeup_delta| has passed.
  // If |soonest_wakeup_delta| is set to base::TimeDelta::Max(), the wake-up
  // task is cancelled.
  static void LaunchBrowserWithWakeUpDelta(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta soonest_wakeup_delta);

  static bool ShouldDisableBackgroundSync();

  // TODO(iclelland): Remove this once the bots have their play services package
  // updated before every test run. (https://crbug.com/514449)
  static void SetPlayServicesVersionCheckDisabledForTests(bool disabled);

  // Fires all pending Background Sync events across all storage partitions
  // for the last used profile.
  // Fires one-shot Background Sync events for registration of |sync_type|.
  void FireBackgroundSyncEvents(
      blink::mojom::BackgroundSyncType sync_type,
      const base::android::JavaParamRef<jobject>& j_runnable);

 private:
  friend struct base::LazyInstanceTraitsBase<BackgroundSyncLauncherAndroid>;

  // Constructor and destructor marked private to enforce singleton
  BackgroundSyncLauncherAndroid();
  ~BackgroundSyncLauncherAndroid();

  void ScheduleBrowserWakeUpImpl(blink::mojom::BackgroundSyncType sync_type);
  void ScheduleBrowserWakeUpWithWakeUpDeltaImpl(
      blink::mojom::BackgroundSyncType sync_type,
      base::TimeDelta soonest_wakeup_delta);

  base::android::ScopedJavaGlobalRef<jobject>
      java_gcm_network_manager_launcher_;
  base::android::ScopedJavaGlobalRef<jobject>
      java_background_sync_background_task_scheduler_launcher_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncLauncherAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
