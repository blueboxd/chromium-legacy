// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/download_startup_utils.h"

#include <jni.h>
#include <utility>

#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/download/android/jni_headers/DownloadStartupUtils_jni.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/download/public/common/in_progress_download_manager.h"

static void JNI_DownloadStartupUtils_EnsureDownloadSystemInitialized(
    JNIEnv* env,
    jboolean is_full_browser_started,
    jboolean is_incognito) {
  DCHECK(is_full_browser_started || !is_incognito)
      << "Incognito mode must load full browser.";
  Profile* profile = nullptr;
  if (is_full_browser_started) {
    profile = ProfileManager::GetActiveUserProfile();
    if (is_incognito) {
      if (profile->HasOffTheRecordProfile())
        profile = profile->GetOffTheRecordProfile();
      else
        return;
    }
  }
  DownloadStartupUtils::EnsureDownloadSystemInitialized(profile);
}

// static
ProfileKey* DownloadStartupUtils::EnsureDownloadSystemInitialized(
    Profile* profile) {
  ProfileKey* profile_key =
      profile ? profile->GetProfileKey()
              : ProfileKeyStartupAccessor::GetInstance()->profile_key();
  DownloadManagerUtils::GetInProgressDownloadManager(profile_key);
  return profile_key;
}
