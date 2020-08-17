// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/chrome_capture_mode_delegate.h"

#include "base/files/file_path.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/login/login_state/login_state.h"

ChromeCaptureModeDelegate::ChromeCaptureModeDelegate() = default;

ChromeCaptureModeDelegate::~ChromeCaptureModeDelegate() = default;

base::FilePath ChromeCaptureModeDelegate::GetActiveUserDownloadsDir() const {
  DCHECK(chromeos::LoginState::Get()->IsUserLoggedIn());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(ProfileManager::GetActiveUserProfile());
  return download_prefs->DownloadPath();
}

void ChromeCaptureModeDelegate::ShowScreenCaptureItemInFolder(
    const base::FilePath& file_path) {
  platform_util::ShowItemInFolder(ProfileManager::GetActiveUserProfile(),
                                  file_path);
}
