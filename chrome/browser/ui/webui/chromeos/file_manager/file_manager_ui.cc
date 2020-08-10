// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/file_manager/file_manager_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace file_manager {

FileManagerUI::FileManagerUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIFileManagerHost);
  source->AddResourcePath("file_manager.css", IDR_FILE_MANAGER_CSS);
  source->AddResourcePath("file_manager.js", IDR_FILE_MANAGER_JS);

  // Default content for chrome://file-manager: ensures unhandled URLs return
  // 404 rather than content from SetDefaultResource().
  source->AddResourcePath("", IDR_FILE_MANAGER_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, source);
}

}  // namespace file_manager
}  // namespace chromeos
