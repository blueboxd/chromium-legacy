// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/download/download_startup_utils.h"

#include <jni.h>
#include <utility>

#include "base/android/path_utils.h"
#include "chrome/android/chrome_jni_headers/DownloadStartupUtils_jni.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/download/download_controller.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/download_offline_content_provider_factory.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"

static void JNI_DownloadStartupUtils_EnsureDownloadSystemInitialized(
    JNIEnv* env,
    jboolean is_full_browser_started) {
  DownloadStartupUtils::EnsureDownloadSystemInitialized(
      is_full_browser_started);
}

// static
void DownloadStartupUtils::EnsureDownloadSystemInitialized(
    bool is_full_browser_started) {
  if (is_full_browser_started)
    return;

  auto* profile_key = ProfileKeyStartupAccessor::GetInstance()->profile_key();
  download::InProgressDownloadManager* in_progress_manager =
      DownloadManagerUtils::GetInProgressDownloadManager(profile_key);
  in_progress_manager->set_download_start_observer(
      DownloadControllerBase::Get());
  in_progress_manager->set_intermediate_path_cb(
      base::BindRepeating(&DownloadTargetDeterminer::GetCrDownloadPath));
  base::FilePath download_dir;
  base::android::GetDownloadsDirectory(&download_dir);
  in_progress_manager->set_default_download_dir(download_dir);
  download::SimpleDownloadManagerCoordinator* coordinator =
      SimpleDownloadManagerCoordinatorFactory::GetForKey(profile_key);
  auto* download_provider =
      DownloadOfflineContentProviderFactory::GetForKey(profile_key);
  download_provider->SetSimpleDownloadManagerCoordinator(coordinator);
}
