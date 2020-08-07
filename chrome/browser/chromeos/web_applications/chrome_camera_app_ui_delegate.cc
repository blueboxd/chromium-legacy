// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_camera_app_ui_delegate.h"

#include "base/files/file_path.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/browser/web_launch/web_launch_files_helper.h"

ChromeCameraAppUIDelegate::ChromeCameraAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeCameraAppUIDelegate::SetLaunchDirectory() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  content::WebContents* web_contents = web_ui_->GetWebContents();

  // Since launch paths does not accept empty vector, we put a placeholder file
  // path in it.
  base::FilePath empty_file_path("/dev/null");

  auto downloads_folder_path =
      file_manager::util::GetDownloadsFolderForProfile(profile);

  web_launch::WebLaunchFilesHelper::SetLaunchDirectoryAndLaunchPaths(
      web_contents, web_contents->GetURL(), downloads_folder_path,
      std::vector<base::FilePath>{empty_file_path});
  web_app::WebAppTabHelper::CreateForWebContents(web_contents);
}
