// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_FILE_MANAGER_FILE_MANAGER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_FILE_MANAGER_FILE_MANAGER_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

namespace chromeos {
namespace file_manager {

// The WebUI controller for chrome::file-manager.
class FileManagerUI : public content::WebUIController {
 public:
  explicit FileManagerUI(content::WebUI* web_ui);

 private:
  DISALLOW_COPY_AND_ASSIGN(FileManagerUI);
};

}  // namespace file_manager
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_FILE_MANAGER_FILE_MANAGER_UI_H_
